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
#include "hwdtb/memattrs.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qom/object.h"
#include "qom/object_interfaces.h"
#include "qapi/error.h"
#include "crypto/secret.h"
#include "system/memory.h"
#include "hw/boards.h"
#include "hw/clock.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/char/serial-mm.h"
#include "hw/block/flash.h"
#include "hw/usb/hcd-dwc3.h"
#include "hw/misc/xlnx-versal-pmc-sysmon.h"
#include "hw/misc/a9scu.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/xlnx_scu_gic.h"
#include "hw/i2c/xlnx-axi-iic.h"
#include "hw/net/xlnx-zynqmp-can.h"
#include "hw/timer/a9gtimer.h"
#include "hw/timer/arm_mptimer.h"
#include "hw/char/xilinx_uartlite.h"
#include "hw/net/cadence_gem.h"
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

/*
 * -- Legacy --
 * This node is inherited from Linux devicetrees and does not transpose well
 * from an hardware perspective. In a Linux devicetree there is one timer node
 * for all the CPU cores.
 *
 * In QEMU/real hardware:
 *     - We have one timer instance per CPU
 *     - The timer is actually embedded into the CPU object (thus the timer IRQs
 *       are exposed by the CPU)
 *     - In heterogeneous systems we can't know for sure what CPUs this
 *       unique timer node is referring to.
 *
 * A hwdtb friendly way to describe this would simply be to connect the lines of
 * each CPU timers back to the GIC explicitly.
 */
static Object *hwdtb_factory_armv8_timer(HwDtbNode *node)
{
    /*
     * We don't have a corresponding device for this node. Create a dummy
     * container, and register a callback to handle it after machine creation.
     */
    hwdtb_node_register_callback(node, HWDTB_PASS_END,
                                 hwdtb_legacy_armv8_timer_connect, NULL);
    return hwdtb_factory_from_oc(node);
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
#ifdef CONFIG_POSIX
    GString *filename;
#endif

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
#ifdef CONFIG_POSIX
        filename = get_legacy_mr_filename(node);
        memory_region_init_ram_from_file(mr, NULL, NULL, size, 0, RAM_SHARED,
                                         filename->str, 0, &error_abort);
        trace_hwdtb_node_memory_region_create_ram_from_file(node->path, size,
                                                            filename->str);
        g_string_free(filename, true);
#else
        error_setg(&error_abort,
                   "hwdtb: file-backed shared memory is not supported on this host");
#endif
        break;
    }

    return OBJECT(mr);
}

/*
 * secret objects need to be parented under /objects and must have the exact
 * name they are given in the DTB. Create a proxy for them.
 * user_creatable_complete must be called on them after the property set pass.
 */
static void hwdtb_user_creatable_call_complete(HwDtbNode *node, void *opaque)
{
    UserCreatable *uc;

    uc = HWDTB_NODE_AS(node, USER_CREATABLE);
    user_creatable_complete(uc, &error_abort);
}

static Object *hwdtb_factory_secret(HwDtbNode *node)
{
    Object *secret, *ret;

    secret = hwdtb_factory_from_oc(node);
    object_property_add_child(object_get_objects_root(),
                              hwdtb_node_get_name(node), secret);
    hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                 hwdtb_user_creatable_call_complete, NULL);

    ret = hwdtb_create_proxy(secret, HWDTB_PROXY_LOCAL_OBJ);

    return ret;
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

static Object *hwdtb_factory_pmc_sysmon(HwDtbNode *node)
{
    hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                 hwdtb_quirk_pmc_sysmon_resolve_phandles, NULL);
    return hwdtb_factory_from_oc(node);
}

static Object *hwdtb_factory_arm_gic(HwDtbNode *node)
{
    hwdtb_node_register_callback_before(node, HWDTB_PASS_SET_PROPERTIES,
                                        hwdtb_legacy_arm_gic_default_prop_values,
                                        NULL);
    return hwdtb_factory_from_oc(node);
}

/*
 * -- Legacy --
 * Warn about confliting CAN bus names, as used in previous documentation
 * versions.
 */
static Object *hwdtb_factory_xlnx_zynqmp_can(HwDtbNode *node)
{
    Object *ret = hwdtb_factory_from_oc(node);
    const GlobalProperty *prop;
    const char *CONFLICTING_PROPS[] = { "canbus0", "canbus1" };
    size_t i;

    for (i = 0; i < ARRAY_SIZE(CONFLICTING_PROPS); i++) {
        prop = qdev_find_global_prop(ret, CONFLICTING_PROPS[i]);

        if (prop && !strcmp(prop->value, CONFLICTING_PROPS[i])) {
            warn_report_once("CAN bus name `%s' conflicts with zynqmp-can `%s' "
                             "property. QEMU will fail. Please change the bus "
                             "name to avoid conflict.", CONFLICTING_PROPS[i],
                             CONFLICTING_PROPS[i]);
        }
    }

    return ret;
}

/*
 * -- Legacy --
 * This allows parsing of a Linux DTB ns16550 node. This is used by some old
 * microblaze dtbs/hwdtbs.
 */
static Object *hwdtb_factory_ns16550(HwDtbNode *node)
{
    uint32_t offset, baudrate;
    HwDtbRegTuple *first_tuple;
    Object *obj;

    first_tuple = hwdtb_node_reg_get_first(node);

    /* Apply the reg-offset property value to the mapping address */
    if ((hwdtb_node_get_prop_uint32(node, "reg-offset", &offset))
        && first_tuple && first_tuple->entry[HWDTB_REG_ADDR].valid) {
        first_tuple->entry[HWDTB_REG_ADDR].val += offset;
    }

    obj = object_new(TYPE_SERIAL_MM);

    if (hwdtb_node_get_prop_uint32(node, "current-speed", &baudrate)) {
        qdev_prop_set_uint32(DEVICE(obj), "baudbase", baudrate);
    }

    /* Hardcoded for Xilinx IPs */
    qdev_prop_set_uint8(DEVICE(obj), "regshift", 2);

    return obj;
}

/*
 * -- Legacy --
 * Handle "cfi-flash" nodes with some hardcoded values (such as the flash ID)
 */
static Object *hwdtb_factory_cfi_flash(HwDtbNode *node)
{
    HwDtbRegTuple *first_tuple;
    uint32_t size, bank_width;
    DeviceState *ret;
    const uint32_t SECTOR_LEN = 64 * KiB;
    const uint16_t FLASH_ID[] = { 0x89, 0x18, 0x0, 0x0 };
    DriveInfo *dinfo;

    first_tuple = hwdtb_node_reg_get_first(node);

    if (!first_tuple) {
        /* No size information, give up */
        return hwdtb_factory_from_oc(node);
    }

    if (!first_tuple->entry[HWDTB_REG_SIZE].valid) {
        /* No size information, give up */
        return hwdtb_factory_from_oc(node);
    }

    size = first_tuple->entry[HWDTB_REG_SIZE].val;

    if (!hwdtb_node_get_prop_uint32(node, "bank-width", &bank_width)) {
        /* No bank width information, give up */
        return hwdtb_factory_from_oc(node);
    }

    ret = qdev_new(TYPE_PFLASH_CFI01);

    qdev_prop_set_uint32(ret, "num-blocks", size / SECTOR_LEN);
    qdev_prop_set_uint64(ret, "sector-length", SECTOR_LEN);
    qdev_prop_set_uint8(ret, "width", bank_width);
    qdev_prop_set_bit(ret, "big-endian", false);
    qdev_prop_set_uint16(ret, "id0", FLASH_ID[0]);
    qdev_prop_set_uint16(ret, "id1", FLASH_ID[1]);
    qdev_prop_set_uint16(ret, "id2", FLASH_ID[2]);
    qdev_prop_set_uint16(ret, "id3", FLASH_ID[3]);
    qdev_prop_set_string(ret, "name", node->path);

    dinfo = drive_get_next(IF_PFLASH);
    if (dinfo) {
        qdev_prop_set_drive(ret, "drive", blk_by_legacy_dinfo(dinfo));
    }

    return OBJECT(ret);
}

/*
 * -- Legacy --
 * Zynq7000 support
 */
static void zynq_connect_cpu_reset(HwDtbNode *node, void *opaque)
{
    CPUState *cpu;
    DeviceState *slcr = HWDTB_NODE_AS(node, DEVICE);
    size_t i = 0;

    CPU_FOREACH(cpu) {
        qemu_irq reset = qdev_get_gpio_in_named(DEVICE(cpu), "reset", 0);

        qdev_connect_gpio_out(slcr, i, reset);
        i++;
    }
}

/*
 * -- Legacy --
 * Zynq7000 support
 */
static Object *hwdtb_factory_zynq_slcr(HwDtbNode *node)
{
    hwdtb_node_register_callback(node, HWDTB_PASS_CONNECT_GPIO,
                                 zynq_connect_cpu_reset, NULL);

    return hwdtb_factory_from_oc(node);
}

static void zynq_set_num_cpu(HwDtbNode *node, void *opque)
{
    DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);

    qdev_prop_set_uint32(dev, "num-cpu", node->hwdtb->num_cpu_found);
}

/*
 * -- Legacy --
 * Zynq7000 support
 */
static Object *hwdtb_factory_zynq_set_num_cpu(HwDtbNode *node)
{
    hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                 zynq_set_num_cpu, NULL);

    return hwdtb_factory_from_oc(node);
}

/*
 * -- Legacy --
 * ZynqMP old dtbs support
 *
 * Add the gpi-sample-mask property on the gpi2 device if missing. This is to
 * ensure backward compatibility with older hwdtbs.
 *
 * This property ensures the STANDBYWFI signals from the APUs get sampled by the
 * iomodule GPI device. This is a workaround to a discrepancy between QEMU WFI
 * ARM instruction implementation and real hardware one.
 */
static void zynqmp_iomod_gpi28_set_sampling_mask(HwDtbNode *node, void *opaque)
{
    DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);

    qdev_prop_set_uint32(dev, "gpi-sample-mask", 0xf);
}

static Object *hwdtb_factory_iomod_gpi_set_sampling_mask(HwDtbNode *node)
{
    HwDtbNode *root = node->hwdtb->root;
    const char *compat;
    bool is_zynqmp = false;

    if (strcmp(hwdtb_node_get_name(node), "pmu_gpi@28")) {
        return hwdtb_factory_from_oc(node);
    }

    compat = hwdtb_node_get_prop_strings(root, "compatible", NULL);

    while (compat) {
        if (!strcmp(compat, "xlnx,zynqmp")) {
            is_zynqmp = true;
            break;
        }

        compat = hwdtb_node_get_prop_strings(root, "compatible", compat);
    }

    if (!is_zynqmp) {
        return hwdtb_factory_from_oc(node);
    }

    if (hwdtb_node_has_prop(node, "gpi-sample-mask")) {
        /* property already present on the node, not a legacy dtb */
        return hwdtb_factory_from_oc(node);
    }

    hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                 zynqmp_iomod_gpi28_set_sampling_mask, NULL);
    return hwdtb_factory_from_oc(node);
}

static void device_set_prop_endianness_little(HwDtbNode *node, void *opaque)
{
    DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);

    qdev_prop_set_enum(dev, "endianness", ENDIAN_MODE_LITTLE);
}

/*
 * -- Legacy --
 * Set the device endianness property to little-endian if not specified in the
 * DTB.
 */
static Object *hwdtb_factory_set_endianness_prop(HwDtbNode *node)
{
    if (!hwdtb_node_has_prop(node, "endianness")) {
        hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                     device_set_prop_endianness_little, NULL);
    }

    return hwdtb_factory_from_oc(node);
}

/*
 * -- Legacy --
 * Turn on the pcs-enabled property on the Cadence GEM model.
 *
 * This property appeared in v10.2 upstream. Previously it was hardcoded on
 * downstream.
 */
static void gem_set_enabled_pcs_prop(HwDtbNode *node, void *opaque)
{
    DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);

    qdev_prop_set_bit(dev, "pcs-enabled", true);
}

static bool is_zynq7000(HwDtbNode *root)
{
    const char *compat;
    bool ret = false;

    compat = hwdtb_node_get_prop_strings(root, "compatible", NULL);

    while (compat) {
        if (!strcmp(compat, "xlnx,zynq-7000")) {
            ret = true;
            break;
        }

        compat = hwdtb_node_get_prop_strings(root, "compatible", compat);
    }

    return ret;
}

static void gem_set_mdio(HwDtbNode *node, void *opaque)
{
    DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);
    uint32_t phy_phandle, reg;
    HwDtbNode *mdio_node;

    if (hwdtb_node_get_prop_uint32(node, "phy-handle", &phy_phandle)) {
        mdio_node = hwdtb_get_node_by_phandle(node->hwdtb, phy_phandle);

        if (mdio_node && !hwdtb_node_has_prop(mdio_node, "compatible")) {
            if (!hwdtb_node_get_prop_nth_uint32(mdio_node,
                                               "reg", 0, &reg)) {
                reg = 0;
            }
            qdev_prop_set_uint32(dev, "phy-addr", reg);
        }
    }
}

static Object *hwdtb_factory_gem_set_props(HwDtbNode *node)
{
    hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                 gem_set_enabled_pcs_prop, NULL);

    if (is_zynq7000(node->hwdtb->root)) {
        hwdtb_node_register_callback(node, HWDTB_PASS_SET_PROPERTIES,
                                     gem_set_mdio, NULL);
    }

    return hwdtb_factory_from_oc(node);
}

static const CompatTranslate STATIC_TRANSLATE_TABLE[] = {
    { "simple-bus", TYPE_MEMORY_REGION },
    { "qemu:memory-region", TYPE_MEMORY_REGION },
    { "ns16550a", "ns16550" },
    { "xlnx,axi-iic-2.0", TYPE_XILINX_AXI_IIC },
    { "xlnx,xps-iic-2.00.a", TYPE_XILINX_AXI_IIC },
    { "arm,cortex-a9-gic", TYPE_ARM_GIC },
    { "qemu:memory-transaction-attr", TYPE_HWDTB_MEMTXATTRS },

    /* Legacy fdt_generic aliases */
    { "xlnx,eth-dma", "xlnx.axi-dma" },
    { "xlnx.zynq-qspi", "xlnx.ps7-qspi" },
    { "arasan,sdhci-8.9a", "xilinx.zynqmp-sdhci" },
    { "xlnx,xps-gpio-1.00.a", "xlnx.axi-gpio" },
    { "xlnx,axi-dpdma-1.0", "xlnx.dpdma" },
    { "xlnx,zynq-can-1.0", "xlnx.zynqmp-can" },
    { "xlnx,ps7-can-1.00.a", "xlnx.zynqmp-can" },
    { "xlnx.ps7-ethernet", "cadence_gem" },
    { "cdns,gem", "cadence_gem" },
    { "cdns,zynq-gem", "cadence_gem" },
    { "cdns,zynqmp-gem", "cadence_gem" },
    { "xlnx.ps7-ttc", "cadence_ttc" },
    { "cdns.ttc", "cadence_ttc" },
    { "cdns.uart", "cadence_uart" },
    { "xlnx.ps7-uart", "cadence_uart" },
    { "cdns.spi-r1p6", "xlnx.ps7-spi" },
    { "xlnx.xuartps", "cadence_uart" },
    { "xilinx_spi", "m25p80" },
    { "silabs,si570", "si57x" },
    { "silabs,si5341", "si5341" },
    { "ethernet-phy-id2000.a231", "dp83867" },
    { "ethernet-phy-id2000.a131", "dp83826" },
    { "ethernet-phy-id0141.0e50", "88e1116" },
    { "ethernet-phy-id0141.0e10", "88e1118r" },
    { "ethernet-phy-id0141.0dd0", "88e1510" },
    { "ethernet-phy-id0283.bc30", "ADIN1300" },
    { "xlnx.microblaze", "microblaze-cpu" },
    { "arm.cortex-a9", "cortex-a9-arm-cpu" },
    { "nxp,pcf8563", "i2c-dev-dummy" },
    { "arm.cortex-a9-twd-timer", TYPE_ARM_MPTIMER },
    { "xlnx.zynq-xadc", "xlnx-zynq-xadc" },
    { "xlnx.ps7-slcr", "xilinx-zynq_slcr" },
    { "xlnx.zynq-slcr", "xilinx-zynq_slcr" },
    { "arm.gic", TYPE_ARM_GIC },
    { "arm.cortex-a9-scu", "a9-scu" },
    { "nxp.pcf8563", "i2c-dev-dummy" },
    { "xilinx.cxtsgen", "arm.generic-timer" },
    { "xlnx.pmc-sysmon", "xlnx-pmc-sysmon" },
    { "xlnx.zynqmp-boot", "xlnx-zynqmp-boot" },
};

static const CompatHandler STATIC_COMPAT_HANDLER[] = {
    { TYPE_MEMORY_REGION, hwdtb_factory_memory_region },
    { "qemu:system-memory", hwdtb_factory_qemu_sysmem },
    { "qemu:memory-region-spec", hwdtb_factory_memory_region_spec },
    { "fixed-clock", hwdtb_factory_fixed_clock },
    { TYPE_QCRYPTO_SECRET, hwdtb_factory_secret },
    { "armv8-timer", hwdtb_factory_armv8_timer },
    { TYPE_USB_DWC3, hwdtb_factory_usb_dwc3 },
    { TYPE_PMC_SYSMON, hwdtb_factory_pmc_sysmon },
    { TYPE_ARM_GIC, hwdtb_factory_arm_gic },
    { TYPE_XLNX_SCU_GIC, hwdtb_factory_arm_gic },
    { TYPE_XLNX_ZYNQMP_CAN, hwdtb_factory_xlnx_zynqmp_can },
    { "ns16550", hwdtb_factory_ns16550 },
    { "cfi-flash", hwdtb_factory_cfi_flash },
    { "xilinx-zynq_slcr", hwdtb_factory_zynq_slcr },
    { TYPE_A9_SCU, hwdtb_factory_zynq_set_num_cpu },
    { TYPE_A9_GTIMER, hwdtb_factory_zynq_set_num_cpu },
    { TYPE_ARM_MPTIMER, hwdtb_factory_zynq_set_num_cpu },
    { "xlnx.io_gpi", hwdtb_factory_iomod_gpi_set_sampling_mask },
    { TYPE_XILINX_UARTLITE, hwdtb_factory_set_endianness_prop },
    { "xlnx.xps-intc", hwdtb_factory_set_endianness_prop },
    { "xlnx.xps-ethernetlite", hwdtb_factory_set_endianness_prop },
    { "xlnx.xps-spi", hwdtb_factory_set_endianness_prop },
    { "xlnx.xps-timer", hwdtb_factory_set_endianness_prop },
    { TYPE_CADENCE_GEM, hwdtb_factory_gem_set_props },
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
