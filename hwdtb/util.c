/*
 * HWDTB utility functions
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/hwdtb.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "hw/qdev-core.h"
#include "system/memory.h"

#include <libfdt.h>

static void hwdtb_walk_node(HwDtbNode *node, void (*fn)(HwDtbNode *node))
{
    HwDtbNode *child;

    fn(node);

    hwdtb_node_foreach_child(child, node) {
        hwdtb_walk_node(child, fn);
    }
}

void hwdtb_walk(HwDtb *hwdtb, void (*fn)(HwDtbNode *node))
{
    hwdtb_walk_node(hwdtb->root, fn);
}

const char *hwdtb_node_get_name(const HwDtbNode *node)
{
    if (node->parent == NULL) {
        return "(root)";
    }

    return fdt_get_name(node->hwdtb->fdt, node->offset, NULL);
}

Object *hwdtb_get_obj(const HwDtbNode *node)
{
    if (object_dynamic_cast(node->obj, TYPE_HWDTB_PROXY)) {
        return object_property_get_link(node->obj, "proxy", &error_abort);
    } else {
        return node->obj;
    }
}

Object *hwdtb_get_parenting_obj(const HwDtbNode *node)
{
    if (hwdtb_proxy_has_flags(node, HWDTB_PROXY_PARENT_ON_OBJ)) {
        return hwdtb_get_obj(node);
    } else {
        return node->obj;
    }
}

const struct fdt_property *hwdtb_node_get_prop(const HwDtbNode *node,
                                               const char *prop_name,
                                               size_t *len)
{
    int offset;

    fdt_for_each_property_offset(offset, node->hwdtb->fdt, node->offset) {
        const struct fdt_property *prop;
        const char *n;
        int l;

        prop = fdt_get_property_by_offset(node->hwdtb->fdt, offset, &l);
        if (prop == NULL) {
            return NULL;
        }

        n = fdt_get_string(node->hwdtb->fdt, fdt32_ld(&prop->nameoff), NULL);

        if (!strcmp(n, prop_name)) {
            if (len) {
                *len = l;
            }
            return prop;
        }
    }

    return NULL;
}

bool hwdtb_node_has_prop(const HwDtbNode *node, const char *prop_name)
{
    return hwdtb_node_get_prop(node, prop_name, NULL) != NULL;
}

bool hwdtb_fdt_prop_parse_uint(const struct fdt_property *prop, size_t skip,
                               size_t max, uint64_t *ret, size_t *lenp)
{
    size_t len = fdt32_to_cpu(prop->len);
    const char *data = prop->data;

    if (skip >= len) {
        return false;
    }

    len -= skip;
    data += skip;

    len = max ?: len;

    switch (len) {
    case 1:
        *ret = prop->data[0];
        break;

    case 2:
        /* fdt16_ld is available in recent libfdt versions only */
        *ret = lduw_be_p((uint16_t *) data);
        break;

    case 4:
        *ret = fdt32_ld((fdt32_t *) data);
        break;

    case 8:
        *ret = fdt64_ld((fdt64_t *) data);
        break;

    default:
        return false;
    }

    if (lenp) {
        *lenp = len;
    }

    return true;
}

bool hwdtb_fdt_prop_parse_uint64(const struct fdt_property *prop,
                                 uint64_t *ret)
{
    size_t len;

    if (!hwdtb_fdt_prop_parse_uint(prop, 0, 0, ret, &len)) {
        return false;
    }

    return len == 8;
}

bool hwdtb_fdt_prop_parse_uint32(const struct fdt_property *prop,
                                 uint32_t *ret)
{
    uint64_t v;
    size_t len;

    if (!hwdtb_fdt_prop_parse_uint(prop, 0, 0, &v, &len)) {
        return false;
    }

    if (len != 4) {
        return false;
    }

    *ret = v;
    return true;
}

bool hwdtb_fdt_prop_parse_nth_uint32(const struct fdt_property *prop,
                                     size_t idx, uint32_t *ret)
{
    uint64_t v;
    size_t len;

    if (!hwdtb_fdt_prop_parse_uint(prop, idx * sizeof(fdt32_t), sizeof(fdt32_t),
                                   &v, &len)) {
        return false;
    }

    if (len != 4) {
        return false;
    }

    *ret = v;
    return true;
}

const char *hwdtb_fdt_prop_parse_string(const struct fdt_property *prop)
{
    const char *ret;
    size_t prop_len = fdt32_ld(&prop->len);

    ret = prop->data;

    if (ret[prop_len - 1] != '\0') {
        return NULL;
    }

    return ret;
}

const char *hwdtb_fdt_prop_parse_next_string(const struct fdt_property *prop,
                                             const char *prev)
{
    size_t len, prev_len;

    if (prev == NULL) {
        return hwdtb_fdt_prop_parse_string(prop);
    }

    len = fdt32_to_cpu(prop->len);
    prev_len = strlen(prev) + 1;

    if (((prev - prop->data) + prev_len) == len) {
        /* reached the last string */
        return NULL;
    }

    return prev + prev_len;
}

bool hwdtb_node_get_prop_uint(const HwDtbNode *node, const char *prop_name,
                              uint64_t *ret)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return false;
    }

    return hwdtb_fdt_prop_parse_uint((void *) prop, 0, 0, ret, NULL);
}

bool hwdtb_node_get_prop_uint64(const HwDtbNode *node, const char *prop_name,
                                uint64_t *val)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return false;
    }

    return hwdtb_fdt_prop_parse_uint64(prop, val);
}

bool hwdtb_node_get_prop_uint32(const HwDtbNode *node, const char *prop_name,
                                uint32_t *val)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return false;
    }

    return hwdtb_fdt_prop_parse_uint32(prop, val);
}

bool hwdtb_node_get_prop_nth_uint32(const HwDtbNode *node,
                                    const char *prop_name, size_t idx,
                                    uint32_t *val)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return false;
    }

    return hwdtb_fdt_prop_parse_nth_uint32(prop, idx, val);
}

const char *hwdtb_node_get_prop_string(const HwDtbNode *node,
                                       const char *prop_name)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return NULL;
    }

    return hwdtb_fdt_prop_parse_string(prop);
}

const char *hwdtb_node_get_prop_strings(const HwDtbNode *node,
                                        const char *prop_name,
                                        const char *prev)
{
    const struct fdt_property *prop;

    prop = hwdtb_node_get_prop(node, prop_name, NULL);

    if (prop == NULL) {
        return NULL;
    }

    return hwdtb_fdt_prop_parse_next_string(prop, prev);
}

bool hwdtb_node_reg_get_first_addr(const HwDtbNode *node, uint64_t *ret)
{
    HwDtbRegTuple *reg = QSIMPLEQ_FIRST(&node->reg);
    HwDtbRegEntry *addr;

    if (reg == NULL) {
        return false;
    }

    addr = &reg->entry[HWDTB_REG_ADDR];

    if (!addr->valid) {
        return false;
    }

    *ret = addr->val;
    return true;
}

bool hwdtb_node_reg_get_first_size(const HwDtbNode *node, uint64_t *ret)
{
    HwDtbRegTuple *reg = QSIMPLEQ_FIRST(&node->reg);
    HwDtbRegEntry *size;

    if (reg == NULL) {
        return false;
    }

    size = &reg->entry[HWDTB_REG_SIZE];

    if (!size->valid) {
        return false;
    }

    *ret = size->val;
    return true;
}

void hwdtb_node_add_child_obj(const HwDtbNode *node, const char *name,
                              Object *child)
{
    g_autofree char *n;

    g_assert(node->obj);

    n = g_strdup_printf("hwdtb-auto<%s>", name);
    object_property_add_child(hwdtb_get_parenting_obj(node), n, child);
}

static bool node_has_gpio(const HwDtbNode *node, const char *name, size_t idx,
                          const char *unnamed_ns, const char *prop_type_prefix)
{
    g_autofree char *propname = NULL;
    DeviceState *dev;
    ObjectProperty *prop;

    dev = HWDTB_NODE_AS(node, DEVICE);

    if (dev == NULL) {
        return NULL;
    }

    name = name ?: unnamed_ns;
    propname = g_strdup_printf("%s[%zu]", name, idx);

    prop = object_property_find(OBJECT(dev), propname);

    if (prop == NULL) {
        return false;
    }

    return strstart(prop->type, prop_type_prefix, NULL);
}

bool hwdtb_node_has_gpio_output(const HwDtbNode *node, const char *name,
                               size_t idx)
{
    return node_has_gpio(node, name, idx, "unnamed-gpio-out", "link<");
}

bool hwdtb_node_has_gpio_input(const HwDtbNode *node, const char *name,
                               size_t idx)
{
    return node_has_gpio(node, name, idx, "unnamed-gpio-in", "child<");
}

HwDtbNode *hwdtb_get_node_by_phandle(HwDtb *hwdtb, uint32_t phandle)
{
    gpointer lookup;

    lookup = g_hash_table_lookup(hwdtb->node_by_phandle,
                                 GINT_TO_POINTER(phandle));

    return (HwDtbNode *) lookup;
}

HwDtbNode *hwdtb_get_node_by_path(HwDtb *hwdtb, const char *path)
{
    gpointer lookup;

    lookup = g_hash_table_lookup(hwdtb->node_by_path, path);

    return (HwDtbNode *) lookup;
}

HwDtbRegTuple *hwdtb_node_reg_get_first(HwDtbNode *hwdtb)
{
    return QSIMPLEQ_FIRST(&hwdtb->reg);
}

uint64_t hwdtb_reg_tuple_val_nofail(const HwDtbRegTuple *tuple,
                                    HwDtbRegEntryKind entry)
{
    g_assert(tuple->entry[entry].valid);

    return tuple->entry[entry].val;
}

uint64_t hwdtb_reg_tuple_val_or(const HwDtbRegTuple *tuple,
                                HwDtbRegEntryKind entry,
                                uint64_t def_value)
{
    if (tuple == NULL) {
        return def_value;
    }

    return tuple->entry[entry].valid ? tuple->entry[entry].val : def_value;
}

uint64_t hwdtb_reg_tuple_val_or_prop_or(HwDtbNode *node, const char *prop,
                                        HwDtbRegTuple *tuple,
                                        HwDtbRegEntryKind kind,
                                        uint64_t def_value)
{
    uint64_t ret;

    if ((tuple != NULL) && (tuple->entry[kind].valid)) {
        return tuple->entry[kind].val;
    } else if (hwdtb_node_get_prop_uint(node, prop, &ret)) {
        return ret;
    } else {
        return def_value;
    }
}

gboolean hwdtb_resolved_gpio_equal(gconstpointer a, gconstpointer b)
{
    const HwDtbResolvedGPIO *ga = a;
    const HwDtbResolvedGPIO *gb = b;

    if (ga->sta != gb->sta) {
        return false;
    }

    if (ga->idx != gb->idx) {
        return false;
    }

    if (ga->name == NULL) {
        return ga->name == gb->name;
    }

    return !strcmp(ga->name, gb->name);
}

guint hwdtb_resolved_gpio_hash(gconstpointer a)
{
    const HwDtbResolvedGPIO *ga = a;
    guint ret = 0;

    if (ga->name) {
        ret += g_str_hash(ga->name);
    }

    ret += ga->idx;
    ret += ga->sta << 16;

    return ret;
}

static HwDtbRegisteredGPIO *
lookup_registered_gpio(HwDtbNode *node, const HwDtbResolvedGPIO *gpio,
                       bool create)
{
    HwDtbRegisteredGPIO *ret;
    GHashTable *hash;

    switch (gpio->sta) {
    case HWDTB_GPIO_OUTPUT:
        hash = node->gpio_output;
        break;

    case HWDTB_GPIO_INPUT:
    case HWDTB_GPIO_LEGACY_INTC:
        hash = node->gpio_input;
        break;

    default:
        g_assert_not_reached();
    }

    ret = g_hash_table_lookup(hash, gpio);

    if ((ret == NULL) && create) {
        ret = g_new0(HwDtbRegisteredGPIO, 1);

        g_hash_table_insert(hash, (gpointer) gpio, ret);
    } else {
        g_assert(ret);
        g_assert(ret->num_conn > 0);
    }

    return ret;
}

void hwdtb_node_register_gpio(HwDtbNode *node, const HwDtbResolvedGPIO *gpio,
                              size_t num_conn)
{
    HwDtbRegisteredGPIO *resolved;

    g_assert(gpio->sta != HWDTB_GPIO_UNRESOLVED);

    if (gpio->sta == HWDTB_GPIO_RESOLUTION_FAILURE) {
        return;
    }

    resolved = lookup_registered_gpio(node, gpio, true);
    resolved->num_conn += num_conn;
}

HwDtbRegisteredGPIO *
hwdtb_node_get_registered_gpio(HwDtbNode *node, const HwDtbResolvedGPIO *gpio)
{
    return lookup_registered_gpio(node, gpio, false);
}

void hwdtb_str_append_tuple(GString *str, const GArray *tuple)
{
    size_t i;
    bool comma;

    comma = str->len && isdigit(str->str[str->len - 1]);

    for (i = 0; i < tuple->len; i++) {
        g_string_append_printf(str, "%s%" PRIu32,
                               comma ? ", " : "",
                               g_array_index(tuple, uint32_t, i));
        comma = true;
    }
}

const char *hwdtb_gpio_get_resolution_str(const HwDtbResolvedGPIO *gpio)
{
    static const char *GPIO_RESO_STR[] = {
        [HWDTB_GPIO_UNRESOLVED] = "unresolved",
        [HWDTB_GPIO_INPUT] = "input",
        [HWDTB_GPIO_OUTPUT] = "output",
        [HWDTB_GPIO_LEGACY_INTC] = "legacy-intc-iface",
        [HWDTB_GPIO_RESOLUTION_FAILURE] = "failure",
    };

    return GPIO_RESO_STR[gpio->sta];
}

bool hwdtb_gpio_is_resolved(const HwDtbResolvedGPIO *gpio)
{
    return gpio->sta != HWDTB_GPIO_UNRESOLVED;
}

void hwdtb_node_register_callback(HwDtbNode *node, HwDtbPass pass,
                                  HwDtbPassCallbackFn fn, void *opaque)
{
    HwDtbPassCallback cb = { .fn = fn, .node = node, .opaque = opaque };

    g_array_append_val(node->hwdtb->callbacks[pass], cb);
}

void hwdtb_node_register_callback_before(HwDtbNode *node, HwDtbPass pass,
                                         HwDtbPassCallbackFn fn, void *opaque)
{
    g_assert(pass > HWDTB_PASS_INSTANTIATE);
    hwdtb_node_register_callback(node, pass - 1, fn, opaque);
}

void hwdtb_call_callbacks(HwDtb *hwdtb, HwDtbPass pass)
{
    size_t i;

    for (i = 0; i < hwdtb->callbacks[pass]->len; i++) {
        HwDtbPassCallback *cb;

        cb = &g_array_index(hwdtb->callbacks[pass], HwDtbPassCallback, i);
        cb->fn(cb->node, cb->opaque);
    }
}

HwDtb *hwdtb_create_machine(MachineState *machine, void *fdt)
{
    HwDtb *hwdtb;
    size_t i;

    hwdtb = g_new0(HwDtb, 1);
    hwdtb->machine = machine;
    hwdtb->fdt = fdt;

    hwdtb->node_by_phandle = g_hash_table_new(NULL, NULL);
    hwdtb->node_by_path = g_hash_table_new(g_str_hash, g_str_equal);
    hwdtb->cpu_clusters = g_hash_table_new(g_str_hash, g_str_equal);
    hwdtb->next_cluster_id = 0;
    hwdtb->next_serial_hd = 0;
    hwdtb->reserved_serial_hd = 0;
    hwdtb->num_cpu_found = 0;

    for (i = 0; i < HWDTB_NUM_PASSES; i++) {
        hwdtb->callbacks[i] = g_array_new(false, false,
                                          sizeof(HwDtbPassCallback));
    }

    memory_region_transaction_begin();

    hwdtb_parse(hwdtb);
    hwdtb_legacy_memory_node_probe(hwdtb);
    hwdtb_resolve(hwdtb);

    hwdtb_instantiate(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_INSTANTIATE);

    hwdtb_set_properties(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_SET_PROPERTIES);

    hwdtb_connect_clocks(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_CONNECT_CLOCK);

    hwdtb_realize_devs(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_REALIZE);

    hwdtb_legacy_mmap_iface(hwdtb);
    hwdtb_mem_map_nodes(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_MEM_MAP);

    hwdtb_gpio_legacy_resolve(hwdtb);
    hwdtb_gpio_resolve(hwdtb);
    hwdtb_gpio_legacy_reverse(hwdtb);
    hwdtb_gpio_register(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_RESOLVE_GPIO);

    hwdtb_connect_gpios(hwdtb);
    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_CONNECT_GPIO);

    hwdtb_call_callbacks(hwdtb, HWDTB_PASS_END);

    memory_region_transaction_commit();

    return hwdtb;
}

void hwdtb_create_machine_oneshot(MachineState *machine, void *fdt)
{
    hwdtb_free(hwdtb_create_machine(machine, fdt));
}

static void hwdtb_conn_free(HwDtbConnection *conn)
{
    size_t i;

    for (i = 0; i < conn->targets->len; i++) {
        HwDtbConnectionTarget *target;

        target = &g_array_index(conn->targets, HwDtbConnectionTarget, i);
        g_array_free(target->tuple, true);
    }

    g_array_free(conn->targets, true);
    g_free(conn);
}

static void hwdtb_node_free_resolved_gpio_ht(GHashTable *ht)
{
    GHashTableIter iter;
    gpointer p_registered_gpio;

    g_hash_table_iter_init(&iter, ht);

    while (g_hash_table_iter_next(&iter, NULL, &p_registered_gpio)) {
        HwDtbRegisteredGPIO *registered_gpio;

        registered_gpio = (HwDtbRegisteredGPIO *) p_registered_gpio;

        if (registered_gpio->cached_descr) {
            g_string_free(registered_gpio->cached_descr, true);
        }
        g_free(registered_gpio);
    }

    g_hash_table_destroy(ht);

}

static void hwdtb_node_free(HwDtbNode *node)
{
    HwDtbNode *child, *child_next;
    HwDtbRegTuple *tuple, *tuple_next;
    HwDtbConnection *conn, *conn_next;
    size_t i;

    QSIMPLEQ_FOREACH_SAFE(child, &node->children, link, child_next) {
        hwdtb_node_free(child);
    }

    hwdtb_node_foreach_reg_tuple_safe(tuple, node, tuple_next) {
        g_free(tuple);
    }

    for (i = 0; i < HWDTB_NUM_CON; i++) {
        hwdtb_node_foreach_connection_safe(conn, node, i, conn_next) {
            hwdtb_conn_free(conn);
        }
    }

    hwdtb_node_free_resolved_gpio_ht(node->gpio_input);
    hwdtb_node_free_resolved_gpio_ht(node->gpio_output);

    g_free(node->path);
    g_free(node);
}

void hwdtb_free(HwDtb *hwdtb)
{
    size_t i;

    g_hash_table_destroy(hwdtb->node_by_phandle);
    g_hash_table_destroy(hwdtb->node_by_path);
    g_hash_table_destroy(hwdtb->cpu_clusters);

    for (i = 0; i < HWDTB_NUM_PASSES; i++) {
        g_array_free(hwdtb->callbacks[i], true);
    }

    hwdtb_node_free(hwdtb->root);
    g_free(hwdtb);
}
