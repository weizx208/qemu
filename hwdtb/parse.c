/*
 * HWDTB parser
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "error.h"
#include "trace.h"

#include <libfdt.h>

typedef struct HwDtbParseCtx {
    GString *path;
    const int *reg_num_cells;
} HwDtbParseCtx;

static const char *NUM_CELLS_PROPS[HWDTB_NUM_REG_KIND] = {
    [HWDTB_REG_ADDR] = "#address-cells",
    [HWDTB_REG_SIZE] = "#size-cells",
    [HWDTB_REG_BUS] = "#bus-cells",
    [HWDTB_REG_PRIO] = "#priority-cells",
};

typedef struct ConnectionFormatDescr {
    const char *id;
    const char *spec;
    const char *spec_extended;
    const char *num_cells;
    const char *parent;
    const char *map;
    const char *map_mask;
    const char *controller;
    const char *names;
    const char *output_names;
} ConnectionFormatDescr;

static const ConnectionFormatDescr CONNECTION_FORMAT[] = {
    [HWDTB_CON_INTERRUPT] = {
        .id = "interrupt",
        .spec = "interrupts",
        .spec_extended = "interrupts-extended",
        .num_cells = "#interrupt-cells",
        .parent = "interrupt-parent",
        .map = "interrupt-map",
        .map_mask = "interrupt-map-mask",
        .controller = "interrupt-controller",
        .names = "interrupt-names",
    },

    [HWDTB_CON_GPIO] = {
        .id = "gpio",
        .spec_extended = "gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
        .names = "gpio-names",
    },

    [HWDTB_CON_POWER_GPIO] = {
        .id = "power-gpio",
        .spec_extended = "power-gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
    },

    [HWDTB_CON_RESET_GPIO] = {
        .id = "reset-gpio",
        .spec_extended = "reset-gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
    },

    [HWDTB_CON_INTERRUPT_GPIO] = {
        .id = "interrupt-gpio",
        .spec_extended = "interrupt-gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
    },

    [HWDTB_CON_ERROR_OUT_GPIO] = {
        .id = "error-out-gpio",
        .spec_extended = "error-out-gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
    },

    [HWDTB_CON_PWR_STATE_GPIO] = {
        .id = "pwr-state-gpio",
        .spec_extended = "pwr-state-gpios",
        .num_cells = "#gpio-cells",
        .controller = "gpio-controller",
    },

    [HWDTB_CON_CLOCK] = {
        .id = "clock",
        .spec_extended = "clocks",
        .num_cells = "#clock-cells",
        .names = "clock-names",
        .output_names = "clock-output-names",
    },
};

typedef struct HwDtbConnectionParseCtx {
    /* nexus: a node with a *-map and a #*-cells properties */
    HwDtbNode *nexus;

    /* a node with a *-parent property */
    HwDtbNode *parent;
} HwDtbConnectionParseCtx;

const char *hwdtb_conn_format_get_spec_extended(HwDtbConnectionKind kind)
{
    return CONNECTION_FORMAT[kind].spec_extended;
}

static bool prop_data_try_consume_cell(const uint8_t **data, size_t *len,
                                       uint32_t *ret)
{
    if (*len < sizeof(fdt32_t)) {
        return false;
    }

    *ret = fdt32_ld((fdt32_t *) *data);

    *len -= sizeof(fdt32_t);
    *data += sizeof(fdt32_t);

    return true;
}

static bool prop_data_try_consume_two_cells(const uint8_t **data, size_t *len,
                                            uint64_t *ret)
{
    if (*len < sizeof(fdt64_t)) {
        return false;
    }

    *ret = fdt64_ld((fdt64_t *) *data);

    *len -= sizeof(fdt64_t);
    *data += sizeof(fdt64_t);

    return true;
}

static bool prop_data_try_consume_tuple(const uint8_t **data, size_t *len,
                                        size_t consume, GArray **ret)
{
    size_t i;

    *ret = NULL;

    if (*len < (consume * sizeof(fdt32_t))) {
        *len = 0;
        return false;
    }

    *ret = g_array_new(false, false, sizeof(uint32_t));

    for (i = 0; i < consume; i++) {
        uint32_t v;

        v = fdt32_ld((fdt32_t *) *data);
        g_array_append_val(*ret, v);

        *data += sizeof(fdt32_t);
        *len -= sizeof(fdt32_t);
    }

    return true;
}

static bool prop_data_try_consume_uint(const uint8_t **data, size_t *len,
                                       size_t consume, uint64_t *ret)
{
    bool r;
    uint32_t ret_u32;

    g_assert(consume <= 2);

    switch (consume) {
    case 0:
        return true;

    case 1:
        r = prop_data_try_consume_cell(data, len, &ret_u32);

        if (r) {
            *ret = ret_u32;
        }

        return r;

    case 2:
        return prop_data_try_consume_two_cells(data, len, ret);

    default:
        g_assert_not_reached();
    }
}

static bool prop_data_try_consume_and_compare_uint(const uint8_t **data,
                                                   size_t *len, size_t consume,
                                                   uint64_t compare,
                                                   uint64_t mask)
{
    uint64_t d = 0;

    if (consume == 0) {
        return true;
    }

    if (!prop_data_try_consume_uint(data, len, consume, &d)) {
        *len = 0;
        return false;
    }

    return ((d & mask) == (compare & mask));
}

static bool prop_data_try_consume_and_compare_tuple(const uint8_t **data,
                                                   size_t *len, size_t consume,
                                                   const GArray *compare,
                                                   const GArray *mask)
{
    size_t i = 0;
    bool res = true;

    while (consume) {
        uint32_t u, c, m;

        if (!prop_data_try_consume_cell(data, len, &u)) {
            *len = 0;
            return false;
        }

        c = g_array_index(compare, uint32_t, i);
        m = g_array_index(mask, uint32_t, i);
        res &= (u & m) == (c & m);
        /* carry on even on compare failure to consume the whole tuple */

        consume--;
        i++;
    }

    return res;
}

static bool prop_data_try_skip_cells(const uint8_t **data,
                                     size_t *len, size_t consume)
{
    if (consume * sizeof(fdt32_t) > *len) {
        *len = 0;
        return false;
    }

    *data += consume * sizeof(fdt32_t);
    *len -= consume * sizeof(fdt32_t);

    return true;
}

static HwDtbNode *hwdtb_node_new(HwDtb *hwdtb, int offset, const GString *path)
{
    HwDtbNode *ret;
    size_t i;

    ret = g_new0(HwDtbNode, 1);
    ret->offset = offset;
    ret->hwdtb = hwdtb;

    if (!*path->str) {
        ret->path = g_strdup("/");
    } else {
        ret->path = g_strdup(path->str);
    }

    QSIMPLEQ_INIT(&ret->children);
    QSIMPLEQ_INIT(&ret->reg);

    for (i = 0; i < HWDTB_NUM_CON; i++) {
        QSIMPLEQ_INIT(&ret->connection[i]);
    }

    return ret;
}

static void add_node_to_hash_tables(HwDtbNode *node)
{
    uint32_t phandle;

    if (hwdtb_node_get_prop_uint32(node, "phandle", &phandle)) {
        trace_hwdtb_node_phandle(node->path, phandle);
        g_hash_table_insert(node->hwdtb->node_by_phandle,
                            GUINT_TO_POINTER(phandle), node);
    }
    g_hash_table_insert(node->hwdtb->node_by_path, (gpointer)node->path, node);
}

/*
 * phandle resolution with error reporting
 */
static HwDtbNode *get_node_by_phandle(HwDtbNode *node, uint32_t phandle,
                                      const char *prop)
{
    HwDtbNode *ret;

    ret = hwdtb_get_node_by_phandle(node->hwdtb, phandle);

    if (ret == NULL) {
        hwdtb_report_err(node, HWDTB_ERR_PHANDLE_NOT_FOUND, prop, phandle);
    }

    return ret;
}

static void parse_reg_num_cells_props(HwDtbNode *node, HwDtbParseCtx *ctx)
{
    size_t i;

    for (i = 0; i < HWDTB_NUM_REG_KIND; i++) {
        uint32_t val;

        if (hwdtb_node_get_prop_uint32(node, NUM_CELLS_PROPS[i],
                                        &val)) {
            node->reg_num_cells[i] = val;

            if (val > 2) {
                hwdtb_report_err(node, HWDTB_ERR_REG_CELLS_TOO_BIG_UNIMP,
                                 NUM_CELLS_PROPS[i]);
            }
        } else {
            node->reg_num_cells[i] = ctx->reg_num_cells[i];
        }
    }
}

static HwDtbNode *hwdtb_parse_node(HwDtb *hwdtb, int offset, HwDtbParseCtx *ctx)
{
    HwDtbNode *node;
    const char *name;
    int subnode;
    const int *parent_num_cells;

    name = fdt_get_name(hwdtb->fdt, offset, NULL);
    g_string_append_printf(ctx->path, "%s", name);

    node = hwdtb_node_new(hwdtb, offset, ctx->path);
    trace_hwdtb_node_parse(node->path);

    parse_reg_num_cells_props(node, ctx);

    g_string_append_c(ctx->path, '/');
    parent_num_cells = ctx->reg_num_cells;
    ctx->reg_num_cells = node->reg_num_cells;

    fdt_for_each_subnode(subnode, hwdtb->fdt, offset) {
        HwDtbNode *child = hwdtb_parse_node(hwdtb, subnode, ctx);

        child->parent = node;
        QSIMPLEQ_INSERT_TAIL(&node->children, child, link);
    }

    ctx->reg_num_cells = parent_num_cells;
    g_string_truncate(ctx->path, ctx->path->len - (strlen(name) + 1));

    add_node_to_hash_tables(node);

    return node;
}

static size_t try_consume_reg_prop_bytes(const uint8_t *prop_data, int len,
                                         HwDtbRegEntry *entry, size_t num_cells)
{
    size_t consumed;

    if (num_cells == 0) {
        entry->valid = false;
        return 0;
    }

    consumed = num_cells * sizeof(uint32_t);

    if (len < consumed) {
        /* not enough remaining bytes in the reg property */
        return len;
    }

    entry->valid = true;

    switch (num_cells) {
    case 1:
        entry->val = fdt32_ld((fdt32_t *) prop_data);
        break;

    default:
        /*
         * More than 2 cells, not supported. Truncate the value to 64 bits. A
         * warning has been emited during the #xxx-cells parsing.
         */
        prop_data += (consumed - sizeof(uint64_t));

        /* fall through */
    case 2:
        entry->val = fdt64_ld((fdt64_t *) prop_data);
        break;
    }

    return consumed;
}

static size_t parse_reg_tuple(HwDtbNode *node, const uint8_t *prop_data,
                              size_t len, HwDtbRegTuple *tuple)
{
    size_t cur = 0, orig_len = len;
    const int *num_cells = node->parent->reg_num_cells;

    if (tuple->extended) {
        uint32_t phandle;

        if (!prop_data_try_consume_cell(&prop_data, &len, &phandle)) {
            /* not enough remaining bytes in the reg property */
            return len;
        }
        tuple->target = get_node_by_phandle(node, phandle, "reg-extended");

        if (tuple->target == NULL) {
            /*
             * Phandle resolution failure. Can't fetch the num-cells of the
             * parent. The parsing of this reg-extended cell cannot continue
             */
            return len;
        }

        num_cells = tuple->target->reg_num_cells;
    }

    while (len && cur < HWDTB_NUM_REG_KIND) {
        size_t consumed;

        consumed = try_consume_reg_prop_bytes(prop_data, len,
                                              &tuple->entry[cur],
                                              num_cells[cur]);
        len -= consumed;
        prop_data += consumed;
        cur++;
    }

    return orig_len - len;
}

static void parse_reg_prop(HwDtbNode *node)
{
    const struct fdt_property *prop;
    size_t len, i = 0;
    const uint8_t *prop_data;
    bool extended = true;

    if (node->parent == NULL) {
        /* a reg property on the root node does not make much sense */
        return;
    }

    /* reg-extended takes precedence over reg */
    prop = hwdtb_node_get_prop(node, "reg-extended", &len);

    if (prop == NULL) {
        prop = hwdtb_node_get_prop(node, "reg", &len);
        extended = false;
    }

    if (prop == NULL) {
        return;
    }

    prop_data = (uint8_t *) prop->data;

    while (len) {
        HwDtbRegTuple *tuple = g_new0(HwDtbRegTuple, 1);
        size_t consumed;

        tuple->idx = i;
        tuple->extended = extended;

        consumed = parse_reg_tuple(node, prop_data, len, tuple);
        g_assert(consumed > 0);

        QSIMPLEQ_INSERT_TAIL(&node->reg, tuple, link);

        len -= consumed;
        prop_data += consumed;
        i++;
    }
}

static void parse_reg_props(HwDtbNode *node)
{
    HwDtbNode *child;

    parse_reg_prop(node);

    hwdtb_node_foreach_child(child, node) {
        parse_reg_props(child);
    }
}

static bool conn_get_parent_info(HwDtbNode *node, HwDtbNode *parent,
                                 HwDtbConnectionKind kind, bool *is_map,
                                 uint32_t *num_cells)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    bool controller_prop, map_prop;

    /* retrieve parent #*-cells property */
    if (!hwdtb_node_get_prop_uint32(parent, descr->num_cells, num_cells)) {
        /*
         * --- Legacy ---
         * Some nodes (ZynqMP IPI nodes) have a custom interrupt-map property
         * but no #interrupt-cells. Remove this hack once those are fixed
         * (return false here instead).
         */
        hwdtb_report_err(node,
                         HWDTB_ERR3(CONN, MISSING_OR_INVAL_PROP, ON_PARENT)
                         ". Defaulting to 1. Please fix the hwdtb.",
                         descr->id, descr->num_cells, parent->path);
        *num_cells = 1;
    }

    if (!descr->controller && !descr->map) {
        /* clocks have no controller nor map nodes */
        *is_map = false;
        return true;
    }

    /* check *-controller or *-map property on parent */
    controller_prop = descr->controller &&
        hwdtb_node_has_prop(parent, descr->controller);
    map_prop = descr->map &&
        hwdtb_node_has_prop(parent, descr->map);

    if (!controller_prop && !map_prop) {
        if (descr->map) {
            hwdtb_report_err(node,
                             HWDTB_ERR3(CONN, MISSING_PROP_OR_PROP, ON_PARENT)
                             ". Assuming controller. Please fix the hwdtb.",
                             descr->id, descr->controller, descr->map,
                             parent->path);
        } else {
            hwdtb_report_err(node, HWDTB_ERR3(CONN, MISSING_PROP, ON_PARENT)
                             ". Assuming controller. Please fix the hwdtb.",
                             descr->id, descr->controller, parent->path);
        }

        /*
         * -- Legacy --
         * Assume this is a controller when we have a num-cells property but no
         * controller nor map property. Existing DTBs should be fixed and this
         * hack removed (return false here instead).
         */
        controller_prop = true;
    }

    *is_map = map_prop;
    return true;
}

static HwDtbNode *conn_get_parent(HwDtbNode *node, HwDtbConnectionKind kind,
                                  const uint8_t **data, size_t *len,
                                  bool extended, HwDtbConnectionParseCtx *ctx,
                                  bool *is_map, uint32_t *num_cells)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    HwDtbNode *parent_node = NULL;
    uint32_t parent;
    HwDtbNode *matched_node = NULL; /* for error reporting */
    const char *matched_prop = NULL;

    /* Find parent */
    if (extended) {
        if (!prop_data_try_consume_cell(data, len, &parent)) {
            /* not enough remaining bytes in the reg property */
            return NULL;
        }
        matched_node = node;
        matched_prop = descr->spec_extended;

    } else if (ctx->parent) {
        if (!hwdtb_node_get_prop_uint32(ctx->parent, descr->parent, &parent)) {
            hwdtb_report_err(node, HWDTB_ERR2(CONN, INVAL_PROP), descr->id,
                             descr->parent);
            return NULL;
        }
        matched_node = ctx->parent;
        matched_prop = descr->parent;
    } else {
        /*
         * No extended specifier, no parent property, consider the nexus node.
         * Note that this is not a standard devicetree behaviour. Only the
         * immediate devicetree parent should normally be considered.
         */
        parent_node = ctx->nexus;

        if (parent_node == NULL) {
            hwdtb_report_err(node, HWDTB_ERR2(CONN, NO_PARENT), descr->id,
                             descr->spec);
            return NULL;
        }
    }

    /* parent phandle resolution in case of *-parent or *-extended */
    if (parent_node == NULL) {
        parent_node = get_node_by_phandle(matched_node, parent, matched_prop);

        if (parent_node == NULL) {
            return NULL;
        }
    }

    if (!conn_get_parent_info(node, parent_node, kind, is_map, num_cells)) {
        return NULL;
    }

    return parent_node;
}

/*
 * Extract the address part and connection part of the map mask property. hwdtb
 * code supports address size <= 2 (32 or 64 bits) only. If it is greater than
 * 2, truncate the address mask value to 64 bits.
 */
static bool conn_map_get_masks(const struct fdt_property *mask_prop,
                               size_t mask_len, uint32_t conn_cells,
                               uint64_t *addr_mask, uint32_t addr_cells,
                               GArray **conn_mask)
{
    const uint8_t *data = (const uint8_t *) mask_prop->data;
    size_t i;

    if (mask_len < (addr_cells + conn_cells) * sizeof(fdt32_t)) {
        return false;
    }

    switch (addr_cells) {
        case 0:
            *addr_mask = 0;
            break;

        case 1:
            *addr_mask = fdt32_ld((fdt32_t *) data);
            data += sizeof(fdt32_t);
            break;

        default:
            data += addr_cells * sizeof(fdt32_t) - sizeof(fdt64_t);

            /* fall through */
        case 2:
            *addr_mask = fdt64_ld((fdt64_t *) data);
            data += sizeof(fdt64_t);
            break;
    }

    *conn_mask = g_array_new(false, false, sizeof(uint32_t));

    for (i = 0; i < conn_cells; i++) {
        uint32_t v = fdt32_ld((fdt32_t *) data);

        g_array_append_val(*conn_mask, v);
        data += sizeof(fdt32_t);
    }

    return true;
}

static bool connection_target_eq(const HwDtbConnectionTarget *a,
                                 const HwDtbConnectionTarget *b)
{
    return ((a->target == b->target) &&
            (a->tuple->len == b->tuple->len) &&
            !memcmp(a->tuple->data, b->tuple->data, a->tuple->len));
}

static bool conn_map_append_target(GArray *targets,
                                   const HwDtbConnectionTarget *target)
{
    size_t i;

    for (i = 0; i < targets->len; i++) {
        const HwDtbConnectionTarget *t;

        t = &g_array_index(targets, HwDtbConnectionTarget, i);
        if (connection_target_eq(t, target)) {
            return false;
        }
    }

    g_array_append_val(targets, *target);
    return true;
}

static void conn_map_append_targets(GArray *targets,
                                    GArray *new_targets)
{
    size_t i;

    for (i = 0; i < new_targets->len; i++) {
        const HwDtbConnectionTarget *t;

        t = &g_array_index(new_targets, HwDtbConnectionTarget, i);

        conn_map_append_target(targets, t);
    }
}

static bool conn_target_expand_map(HwDtbNode *node, HwDtbConnectionTarget *map,
                                   HwDtbConnectionKind kind, GArray **ret)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    const struct fdt_property *map_prop, *map_mask_prop;
    const uint8_t *map_data;
    HwDtbNode *target = map->target;
    size_t map_len, map_mask_len;
    uint32_t addr_cells, conn_cells;
    uint64_t addr_mask, node_addr;
    g_autoptr(GArray) conn_mask = NULL;
    bool addr_missing_reported = false;

    *ret = NULL;

    map_prop = hwdtb_node_get_prop(target, descr->map, &map_len);
    g_assert(map_prop != NULL);

    map_mask_prop = hwdtb_node_get_prop(target, descr->map_mask, &map_mask_len);

    if (map_mask_prop == NULL) {
        hwdtb_report_err(target, HWDTB_ERR2(CONN, MISSING_PROP), descr->id,
                         descr->map_mask);
        return false;
    }

    if (!hwdtb_node_get_prop_uint32(target, descr->num_cells, &conn_cells)) {
        hwdtb_report_err(target, HWDTB_ERR2(CONN, MISSING_PROP)
                         ". Defaulting to 1. Please fix the hwdtb.", descr->id,
                         descr->num_cells);
        /*
         * --- Legacy ---
         * Some nodes (ZynqMP IPI nodes) have a custom interrupt-map property
         * but no #interrupt-cells. Remove this hack once those are fixed
         * (return false here instead).
         */
        conn_cells = 1;
    }

    addr_cells = node->parent->reg_num_cells[HWDTB_REG_ADDR];

    if (!conn_map_get_masks(map_mask_prop, map_mask_len, conn_cells, &addr_mask,
                            addr_cells, &conn_mask)) {
        hwdtb_report_err(target, HWDTB_ERR_UNEXPECTED_END_OF_PROP,
                         descr->map_mask);
        return false;
    }

    map_data = (uint8_t *) map_prop->data;

    if (!hwdtb_node_reg_get_first_addr(node, &node_addr)) {
        /*
         * Don't bail out here. Set the address mask to 0 so that the addresses
         * get ignored in the mapping. Hwdtbs usually don't care about the
         * address value of the node in interrupt-map mappings. This will allow
         * matching of nodes without a reg property.
         */
        addr_mask = 0;
    }

    *ret = g_array_new(false, false, sizeof(HwDtbConnectionTarget));

    while (map_len) {
        bool addr_match, conn_match, is_map;
        uint32_t parent_phandle, parent_conn_cells, parent_addr_cells;
        HwDtbNode *parent;
        HwDtbConnectionTarget resolved_target;

        memset(&resolved_target, 0, sizeof(resolved_target));

        addr_match = prop_data_try_consume_and_compare_uint(&map_data, &map_len,
                                                            addr_cells,
                                                            node_addr,
                                                            addr_mask);

        conn_match = prop_data_try_consume_and_compare_tuple(&map_data,
                                                             &map_len,
                                                             conn_cells,
                                                             map->tuple,
                                                             conn_mask);

        if (!prop_data_try_consume_cell(&map_data, &map_len, &parent_phandle)) {
            hwdtb_report_err(target, HWDTB_ERR_UNEXPECTED_END_OF_PROP,
                             descr->map);
            break;
        }

        parent = get_node_by_phandle(target, parent_phandle, descr->map);

        if (parent == NULL || !conn_get_parent_info(target, parent, kind,
                                                    &is_map, &parent_conn_cells)) {
            /*
             * phandle resolution failed or invalid parent. Thus we can't get
             * #*-cells value. We can't continue parsing the map property has we
             * don't know how much cells to skip to get to the next tuple.
             */
            break;
        }

        if (!hwdtb_node_get_prop_uint32(parent, NUM_CELLS_PROPS[HWDTB_REG_ADDR],
                                        &parent_addr_cells)) {
            /*
             * Same as above, we can't know how much to skip. However since this
             * feature is unused in hwdtbs, default to 0 in this case to avoid
             * breaking bad existing dtbs.
             */
            if (!addr_missing_reported) {
                hwdtb_report_err(node, HWDTB_ERR3(CONN, MISSING_OR_INVAL_PROP,
                                                  ON_PARENT)
                                 ". Defaulting to 0.",
                                 descr->id, NUM_CELLS_PROPS[HWDTB_REG_ADDR],
                                 parent->path);
                addr_missing_reported = true; /* avoid spamming the error */
            }

            parent_addr_cells = 0;
        }

        /*
         * From here we know how much to consume to skip the rest of the tuple
         * in case matching failed.
         */
        if (!addr_match || !conn_match) {
            if (!prop_data_try_skip_cells(&map_data, &map_len,
                                          parent_addr_cells + parent_conn_cells)) {
                hwdtb_report_err(target, HWDTB_ERR_UNEXPECTED_END_OF_PROP,
                                 descr->map);
            }
            continue;
        }

        /*
         * We don't care about the target address value in the mapping
         * (#address-cells is 0 in all practical use-cases).
         */
        if (!prop_data_try_skip_cells(&map_data, &map_len, parent_addr_cells)) {
            hwdtb_report_err(target, HWDTB_ERR_UNEXPECTED_END_OF_PROP,
                             descr->map);
            continue;
        }

        resolved_target.target = parent;
        if (prop_data_try_consume_tuple(&map_data, &map_len, parent_conn_cells,
                                        &resolved_target.tuple)) {
            bool append_success = true;

            if (is_map) {
                g_autoptr(GArray) expanded;

                /*
                 * Perform a recursive mapping resolution and append the result.
                 */
                conn_target_expand_map(parent, &resolved_target, kind,
                                       &expanded);

                if (expanded) {
                    conn_map_append_targets(*ret, expanded);
                }

                g_array_free(resolved_target.tuple, true);
            } else {
                append_success = conn_map_append_target(*ret, &resolved_target);
            }

            if (!append_success ||
                trace_event_get_state_backends(TRACE_HWDTB_NODE_PARSE_CONN_MAP_RESOLVE)) {
                g_autoptr(GString) src_tuple = g_string_new("");
                g_autoptr(GString) target_tuple = g_string_new("");

                if (addr_mask) {
                    g_string_append_printf(src_tuple, "0x%" PRIx64, node_addr);
                }

                hwdtb_str_append_tuple(src_tuple, map->tuple);

                if (parent_addr_cells) {
                    g_string_append(target_tuple, "(ignored addr)");
                }

                hwdtb_str_append_tuple(target_tuple, resolved_target.tuple);

                if (append_success) {
                    trace_hwdtb_node_parse_conn_map_resolve(
                        node->path, map->target->path, descr->map,
                        src_tuple->str, resolved_target.target->path,
                        target_tuple->str);
                } else {
                    hwdtb_report_err(
                        node, HWDTB_ERR3(CONN, DUPLICATE_MAP_ENTRY, ON_PARENT),
                        descr->id, descr->map, src_tuple->str, target->path);
                }
            }
        }
    }

    return true;
}

static void conn_expand_map(HwDtbNode *node, HwDtbConnection *con)
{
    HwDtbConnectionTarget *target;
    GArray *expanded;

    g_assert(con->targets->len == 1);
    target = &g_array_index(con->targets, HwDtbConnectionTarget, 0);

    conn_target_expand_map(node, target, con->kind, &expanded);

    g_array_free(target->tuple, true);
    g_array_free(con->targets, true);

    con->targets = expanded;
}

static HwDtbConnectionTarget *conn_append_target(HwDtbConnection *con,
                                                 HwDtbNode *target,
                                                 GArray *tuple)
{
    HwDtbConnectionTarget *t;

    g_array_set_size(con->targets, con->targets->len + 1);

    t = &g_array_index(con->targets, HwDtbConnectionTarget,
                       con->targets->len - 1);
    t->target = target;
    t->tuple = tuple;

    return t;
}

/*
 * Only used by clocks. Parses the clock-output-names on the "controller" (clock
 * source).
 */
static void target_fill_name(HwDtbConnectionTarget *target,
                             HwDtbConnectionKind kind,
                             uint32_t num_cells)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    HwDtbNode *ctrl = target->target;
    size_t i = 0, idx;
    const char *name;

    if (descr->output_names == NULL) {
        return;
    }

    if (num_cells != 1) {
        return;
    }

    idx = g_array_index(target->tuple, uint32_t, 0);
    name = hwdtb_node_get_prop_strings(ctrl, descr->output_names, NULL);

    while (name && i < idx) {
        name = hwdtb_node_get_prop_strings(ctrl, descr->output_names, name);
        i++;
    }

    if (name == NULL) {
        return;
    }

    target->name = name;
}

static size_t parse_one_conn_specifier(HwDtbNode *node,
                                       HwDtbConnectionKind kind,
                                       const uint8_t *data, size_t len,
                                       bool extended, size_t spec_index,
                                       HwDtbConnectionParseCtx *ctx,
                                       HwDtbConnection **ret)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    HwDtbConnectionTarget *t;
    size_t i, len_save = len;
    HwDtbNode *parent_node = NULL;
    uint32_t num_cells;
    bool is_map;
    GArray *tuple;

    *ret = NULL;

    parent_node = conn_get_parent(node, kind, &data, &len,
                                  extended, ctx, &is_map, &num_cells);

    if (parent_node == NULL) {
        return len_save;
    }

    tuple = g_array_new(false, false, sizeof(uint32_t));

    for (i = 0; i < num_cells; i++) {
        uint32_t cell;

        if (!prop_data_try_consume_cell(&data, &len, &cell)) {
            hwdtb_report_err(node, HWDTB_ERR2(CONN, SPEC_NOT_ENOUGH_CELLS),
                             descr->id,
                             extended ? descr->spec_extended : descr->spec,
                             descr->num_cells, num_cells);
            g_array_free(tuple, true);
            return len_save;
        }

        /*
         * -- Legacy --
         * Clear the "and" bit on the cell. This is not supported.
         */
        cell &= ~(1 << 31);
        g_array_append_val(tuple, cell);
    }

    *ret = g_new0(HwDtbConnection, 1);
    (*ret)->kind = kind;
    (*ret)->targets = g_array_new(false, true, sizeof(HwDtbConnectionTarget));
    (*ret)->idx = spec_index;

    t = conn_append_target(*ret, parent_node, tuple);
    target_fill_name(t, kind, num_cells);

    if (trace_event_get_state_backends(TRACE_HWDTB_NODE_PARSE_CONNECTION)) {
        g_autoptr(GString) tuple_str = g_string_new("");

        hwdtb_str_append_tuple(tuple_str, tuple);
        trace_hwdtb_node_parse_connection(node->path, descr->id, spec_index,
                                          parent_node->path, tuple_str->str);
    }

    if (is_map) {
        conn_expand_map(node, *ret);
    }

    return len_save - len;
}

static void conns_fill_name(HwDtbNode *node, HwDtbConnectionKind kind)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    HwDtbConnection *conn;
    const char *name;

    if (descr->names == NULL) {
        return;
    }

    if (!hwdtb_node_has_prop(node, descr->names)) {
        return;
    }

    name = hwdtb_node_get_prop_strings(node, descr->names, NULL);
    hwdtb_node_foreach_connection(conn, node, kind) {
        if (name == NULL) {
            break;
        }

        conn->name = name;
        name = hwdtb_node_get_prop_strings(node, descr->names, name);
    }
}

static void parse_connection_spec(HwDtbNode *node, HwDtbConnectionKind kind,
                                  HwDtbConnectionParseCtx *ctx)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    const struct fdt_property *prop;
    size_t len, i = 0;
    const uint8_t *data;
    HwDtbConnection *conn;
    bool extended = true;

    prop = hwdtb_node_get_prop(node, descr->spec_extended, &len);

    if (prop == NULL && descr->spec) {
        prop = hwdtb_node_get_prop(node, descr->spec, &len);
        extended = false;
    }

    if (prop == NULL) {
        return;
    }

    data = (uint8_t *) prop->data;

    while (len) {
        size_t consumed;

        consumed = parse_one_conn_specifier(node, kind, data, len, extended,
                                            i, ctx, &conn);

        if (conn) {
            QSIMPLEQ_INSERT_TAIL(&node->connection[kind], conn, link);
        }

        len -= consumed;
        data += consumed;
        i++;
    }

    conns_fill_name(node, kind);
}

static void parse_connection(HwDtbNode *node, HwDtbConnectionKind kind,
                             HwDtbConnectionParseCtx *ctx)
{
    const ConnectionFormatDescr *descr = &CONNECTION_FORMAT[kind];
    bool update_nexus_ctx, update_parent_ctx;
    HwDtbNode *nexus_ctx_save, *parent_ctx_save, *child;

    /* -- Legacy --
     * The nexus node should be the immediate parent of the node (or, in case of
     * interrupt-parent/interrupts-extended, the specified node). Existing
     * hwdtbs have:
     *    - a big interrupt-map near the root with all the IRQs,
     *    - some nodes with a interrupt-map for themselves, but no
     *      interrupt-parent or interrupts-extended, only interrupts.
     *
     * So we need to considere the whole hierarchy when looking for the nexus
     * node, _including_ the current node. This second case should really be
     * fixed in hwdtbs (the sole users of this are the ZynqMP IPI nodes).
     */

    /* check if this node has a *-map property and #*-cells property */
    update_nexus_ctx =
        (descr->map && hwdtb_node_has_prop(node, descr->map))
        /* -- Legacy --
         * Some nodes (ZynqMP IPI nodes) have a custom interrupt-map property
         * but no #interrupt-cells. Restore this check once those are fixed
         * (remove the true ||).
         */
        && (true ||
            (descr->num_cells && hwdtb_node_has_prop(node, descr->num_cells)));

    update_parent_ctx =
        descr->parent && hwdtb_node_has_prop(node, descr->parent);

    if (update_nexus_ctx) {
        nexus_ctx_save = ctx->nexus;
        ctx->nexus = node;
    }

    if (update_parent_ctx) {
        parent_ctx_save = ctx->parent;
        ctx->parent = node;
    }

    parse_connection_spec(node, kind, ctx);

    hwdtb_node_foreach_child(child, node) {
        parse_connection(child, kind, ctx);
    }

    if (update_nexus_ctx) {
        ctx->nexus = nexus_ctx_save;
    }

    if (update_parent_ctx) {
        ctx->parent = parent_ctx_save;
    }
}

static void parse_connections(HwDtbNode *node)
{
    size_t i;

    for (i = 0; i < HWDTB_NUM_CON; i++) {
        HwDtbConnectionParseCtx ctx = { .nexus = NULL, .parent = NULL };

        parse_connection(node, i, &ctx);
    }
}

void hwdtb_parse(HwDtb *hwdtb)
{
    static const int DEFAULT_REG_NUM_CELLS[] = {
        /* Legacy default values */
        [HWDTB_REG_ADDR] = 1,
        [HWDTB_REG_SIZE] = 1,
        [HWDTB_REG_BUS] = 0,
        [HWDTB_REG_PRIO] = 0,
    };

    int root_offset = fdt_next_node(hwdtb->fdt, -1, NULL);
    HwDtbParseCtx ctx = {
        .path = g_string_new(""),
        .reg_num_cells = DEFAULT_REG_NUM_CELLS,
    };

    hwdtb->root = hwdtb_parse_node(hwdtb, root_offset, &ctx);

    g_string_free(ctx.path, true);

    parse_reg_props(hwdtb->root);
    parse_connections(hwdtb->root);
}
