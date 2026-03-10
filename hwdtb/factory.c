/*
 * HWDTB node factories
 *
 * Those functions are used during the creation phase. They return an instance
 * of a QOM object. The factory function selection for a given node is the
 * result of the resolve pass.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qom/object.h"
#include "qapi/error.h"
#include "system/memory.h"
#include "hw/boards.h"
#include "hw/clock.h"
#include "hw/sysbus.h"
#include "hw/usb/hcd-dwc3.h"
#include "error.h"
#include "trace.h"

typedef struct CompatTranslate {
    const char *from;
    const char *to;
} CompatTranslate;

typedef struct CompatHandler {
    const char *compat;
    HwDtbObjectFactory factory;
} CompatHandler;

static Object *hwdtb_factory_from_oc(HwDtbNode *node)
{
    return object_new_with_class(node->oc);
}

static Object *hwdtb_factory_qemu_sysmem(HwDtbNode *node)
{
    Object *obj;

    obj = hwdtb_create_proxy(OBJECT(get_system_memory()), 0);
    return obj;
}

static uint64_t prop_or(HwDtbNode *node, const char *prop, uint64_t def_value)
{
    uint64_t ret;

    if (hwdtb_node_get_prop_uint(node, prop, &ret)) {
        return ret;
    } else {
        return def_value;
    }
}

typedef struct AliasSbdRegionParam {
    HwDtbNode *aliased;
    uint32_t idx;
} AliasSbdRegionParam;

static void do_alias_sbd_region(HwDtbNode *node, void *opaque)
{
    AliasSbdRegionParam *params = (AliasSbdRegionParam *) opaque;
    SysBusDevice *sbd;
    MemoryRegion *mr, *aliased_mr;

    /*
     * We are after the realize pass so we can query the MR on the sysbus
     * device.
     */
    sbd = SYS_BUS_DEVICE(hwdtb_get_obj(params->aliased));
    aliased_mr = sysbus_mmio_get_region(sbd, params->idx);

    if (aliased_mr == NULL) {
        return;
    }

    /*
     * It is not ideal to poke the private members of the MR. At this
     * point it is already created as a MR container. Since it's not mapped yet
     * (the memory map pass is not done yet) it is OK to set its alias field
     * now.
     */
    mr = HWDTB_NODE_AS(node, MEMORY_REGION);
    mr->alias = aliased_mr;
    mr->alias_offset = 0;

    trace_hwdtb_node_memory_region_alias_sbd_region(node->path,
                                                    params->aliased->path,
                                                    params->idx);

    g_free(params);
}

static HwDtbNode *hwdtb_memory_region_get_aliased_mr(HwDtbNode *node)
{
    MemoryRegion *aliased_mr;
    uint32_t phandle;
    HwDtbNode *aliased;

    if (!hwdtb_node_get_prop_nth_uint32(node, "alias", 0, &phandle)) {
        hwdtb_report_err(node, HWDTB_ERR_INVAL_PROP, "alias");
        return NULL;
    }

    aliased = hwdtb_get_node_by_phandle(node->hwdtb, phandle);

    if (aliased == NULL) {
        hwdtb_report_err(node, HWDTB_ERR_PHANDLE_NOT_FOUND, "alias", phandle);
        return NULL;
    }

    if (!aliased->obj) {
        /* dependency: we need to create the aliased MR first. */
        /* TODO: cycle detection */
        aliased->obj = aliased->factory(aliased);
    }

    aliased_mr = HWDTB_NODE_AS(aliased, MEMORY_REGION);

    if (aliased_mr == NULL) {
        AliasSbdRegionParam *params;
        uint32_t idx;

        /*
         * Legacy fdt_generic code allows to have a sysbus device here as well.
         * In this case the second cell value is the sysbus device memory region
         * index. Here however we are at object creation time. We must wait for
         * the realize pass to be done to be able to access sysbus device memory
         * regions. Register a callback to do so and return here to create a
         * regular memory region.
         */

        if (!HWDTB_NODE_AS(aliased, SYS_BUS_DEVICE)) {
            hwdtb_report_err(node, HWDTB_ERR_TYPE_MISMATCH_2,
                             TYPE_MEMORY_REGION, TYPE_SYS_BUS_DEVICE);
            return NULL;
        }

        if (!hwdtb_node_get_prop_nth_uint32(node, "alias", 1, &idx)) {
            hwdtb_report_err(node, HWDTB_ERR_UNEXPECTED_END_OF_PROP, "alias");
            return NULL;
        }

        params = g_new(AliasSbdRegionParam, 1);
        params->aliased = aliased;
        params->idx = idx;
        hwdtb_node_register_callback(node, HWDTB_PASS_REALIZE,
                                     do_alias_sbd_region, params);
        return NULL;
    }

    return aliased;
}

static char *get_escaped_node_path(HwDtbNode *node)
{
    char *ret = g_strdup(node->path);
    char *slash = ret;

    while ((slash = strchr(slash, '/'))) {
        *slash = '_';
    }

    return ret;
}

static GString *get_legacy_mr_filename(HwDtbNode *node)
{
    GString *ret = g_string_new("");
    char *escaped_path;

    if (machine_path) {
        g_string_append(ret, machine_path);
    } else {
        g_string_append(ret, ".");
    }
    g_string_append_c(ret, G_DIR_SEPARATOR);
    g_string_append(ret, "qemu-memory-");

    escaped_path = get_escaped_node_path(node);
    g_string_append(ret, escaped_path);
    g_free(escaped_path);

    return ret;
}

/*
 * Create a MemoryRegion by parsing the necessary properties on the node.
 * This factory is needed because memory regions are not very well QOMified. We
 * still need to rely on the memory_region_* API to create them correctly
 * (especially the memory_region_init_* functions). We also need to support
 * legacy properties (qemu,ram, ...)
 */
static Object *hwdtb_factory_memory_region(HwDtbNode *node)
{
    HwDtbRegTuple *tuple = NULL;
    HwDtbNode *aliased = NULL;
    MemoryRegion *mr, *aliased_mr = NULL;
    uint64_t size;
    GString *filename;

    /*
     * -- Legacy --
     * The possible values of the legacy qemu,ram property on memory regions.
     */
    enum {
        HWDTB_MR_CONTAINER = 0,
        HWDTB_MR_RAM = 1,
        HWDTB_MR_RAM_FROM_FILE = 2,
    } mr_mode;

    if (node->obj) {
        /*
         * The factory already run on this node because of MR alias dependency.
         */
        return node->obj;
    }

    if (!QSIMPLEQ_EMPTY(&node->reg)) {
        tuple = QSIMPLEQ_FIRST(&node->reg);
    }

    size = hwdtb_reg_tuple_val_or_prop_or(node, "size",
                                          tuple, HWDTB_REG_SIZE, 0);
    mr_mode = prop_or(node, "ram", HWDTB_MR_CONTAINER);

    if (hwdtb_node_has_prop(node, "alias")) {
        aliased = hwdtb_memory_region_get_aliased_mr(node);

        if (aliased) {
            aliased_mr = MEMORY_REGION(hwdtb_get_obj(aliased));
        }
    }

    mr = g_new0(MemoryRegion, 1);

    switch (mr_mode) {
    default:
        hwdtb_report_err(node, "Invalid value `%d' for `ram' property. "
                         "Defaulting to 0 (MR container)",
                         mr_mode);

        /* fall through */
    case HWDTB_MR_CONTAINER:
        if (!size) {
            /*
             * -- Legacy --
             * Missing size specifier defaults to UINT64_MAX for a container
             * MR.
             */
            size = UINT64_MAX;
        }

        if (aliased_mr) {
            memory_region_init_alias(mr, NULL, NULL, aliased_mr, 0, size);
            trace_hwdtb_node_memory_region_create_alias(node->path, size,
                                                        aliased->path);
        } else {
            memory_region_init(mr, NULL, NULL, size);
            trace_hwdtb_node_memory_region_create(node->path, size);
        }
        break;

    case HWDTB_MR_RAM:
        memory_region_init_ram_nomigrate(mr, NULL, NULL, size, &error_abort);
        trace_hwdtb_node_memory_region_create_ram(node->path, size);
        break;

    case HWDTB_MR_RAM_FROM_FILE:
        filename = get_legacy_mr_filename(node);
        memory_region_init_ram_from_file(mr, NULL, NULL, size, 0, RAM_SHARED,
                                         filename->str, 0, &error_abort);
        trace_hwdtb_node_memory_region_create_ram_from_file(node->path, size,
                                                            filename->str);
        g_string_free(filename, true);
        break;
    }

    return OBJECT(mr);
}

/*
 * -- Legacy --
 * Create additional RAM memory regions based on the value of -m command line
 * argument.
 */
static Object *hwdtb_factory_memory_region_spec(HwDtbNode *node)
{
    HwDtb *hwdtb = node->hwdtb;
    MachineState *machine = hwdtb->machine;
    uint64_t node_size, max_size;
    HwDtbRegTuple *tuple;

    if (hwdtb->fulfilled_ram_amount >= machine->ram_size) {
        /*
         * We already have the required amount of RAM. Create a dummy container.
         */
        return hwdtb_factory_from_oc(node);
    }

    if (!hwdtb_node_reg_get_first_size(node, &node_size)) {
        /*
         * Legacy code has no support for the size property here. reg is
         * required.
         */
        return hwdtb_factory_from_oc(node);
    }

    max_size = MIN(machine->ram_size - hwdtb->fulfilled_ram_amount, node_size);

    tuple = hwdtb_node_reg_get_first(node);
    tuple->entry[HWDTB_REG_SIZE].val = max_size;

    hwdtb->fulfilled_ram_amount += max_size;
    return hwdtb_factory_memory_region(node);
}

static Object *hwdtb_factory_fixed_clock(HwDtbNode *node)
{
    uint64_t clock_freq;
    Object *ret;

    if (!hwdtb_node_get_prop_uint(node, "clock-frequency", &clock_freq)) {
        clock_freq = 0;
        hwdtb_report_err(node, HWDTB_ERR2(MISSING_PROP, DEFAULT_VAL_U64),
                         "clock-frequency", clock_freq);
    }

    /* FIXME: clock should be created with clock_new. Need the parent */
    ret = object_new(TYPE_CLOCK);
    clock_set_hz(CLOCK(ret), clock_freq);

    return ret;
}

/*
 * -- Legacy --
 * For usb_dwc3 devices, some old DTBs specify two reg tuples. The first one must
 * be ignored.
 */
static Object *hwdtb_factory_usb_dwc3(HwDtbNode *node)
{
    size_t i = 0;
    HwDtbRegTuple *first_tuple, *tuple, *next;

    hwdtb_node_foreach_reg_tuple_safe(tuple, node, next) {
        if (i == 0) {
            first_tuple = tuple;
        }

        if (i == 1) {
            memcpy(first_tuple->entry, tuple->entry, sizeof(tuple->entry));
            first_tuple->extended = tuple->extended;
            first_tuple->target = tuple->target;
        }

        if (i) {
            QSIMPLEQ_REMOVE(&node->reg, tuple, HwDtbRegTuple, link);
        }

        i++;
    }

    return hwdtb_factory_from_oc(node);
}

static const CompatTranslate STATIC_TRANSLATE_TABLE[] = {
    { "simple-bus", TYPE_MEMORY_REGION },
    { "qemu:memory-region", TYPE_MEMORY_REGION },
};

static const CompatHandler STATIC_COMPAT_HANDLER[] = {
    { TYPE_MEMORY_REGION, hwdtb_factory_memory_region },
    { "qemu:system-memory", hwdtb_factory_qemu_sysmem },
    { "qemu:memory-region-spec", hwdtb_factory_memory_region_spec },
    { "fixed-clock", hwdtb_factory_fixed_clock },
    { TYPE_USB_DWC3, hwdtb_factory_usb_dwc3 },
};

const char *hwdtb_compat_translate(const char *compat)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(STATIC_TRANSLATE_TABLE); i++) {
        if (!strcmp(compat, STATIC_TRANSLATE_TABLE[i].from)) {
            return STATIC_TRANSLATE_TABLE[i].to;
        }
    }

    return NULL;
}

HwDtbObjectFactory hwdtb_get_factory_for_compat(const char *compat)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(STATIC_COMPAT_HANDLER); i++) {
        if (!strcmp(compat, STATIC_COMPAT_HANDLER[i].compat)) {
            return STATIC_COMPAT_HANDLER[i].factory;
        }
    }

    return NULL;
}

HwDtbObjectFactory hwdtb_get_default_factory(void)
{
    return hwdtb_factory_from_oc;
}
