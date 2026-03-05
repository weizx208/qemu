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

static HwDtbNode *hwdtb_node_new(HwDtb *hwdtb, int offset, const GString *path)
{
    HwDtbNode *ret;

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
}
