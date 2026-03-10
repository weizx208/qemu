/*
 * HWDTB devices realization
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/hwdtb.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "system/reset.h"
#include "hw/qdev-properties.h"
#include "hw/core/cpu.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/i2c/i2c.h"
#include "hw/i3c/i3c.h"
#include "hw/mdio/mdio_slave.h"
#include "hw/block/ufshc-if.h"
#include "hw/misc/unimp.h"
#include "error.h"
#include "trace.h"

#include <libfdt.h>

static void ssi_target_connect_cs_gpio(HwDtbNode *node, void *opaque)
{
    DeviceState *dev, *parent;
    HwDtbRegTuple *tuple;
    qemu_irq cs_gpio;
    int cs_line, bus;
    const char *cs_namespace = NULL;
    g_autoptr(GString) out_descr = NULL;
    g_autoptr(GString) in_descr = NULL;

    dev = DEVICE(hwdtb_get_obj(node));
    parent = DEVICE(hwdtb_get_obj(node->parent));

    g_assert(hwdtb_node_has_gpio_input(node, SSI_GPIO_CS, 0));

    tuple = hwdtb_node_reg_get_first(node);

    if (tuple == NULL) {
        bus = 0;
    } else {
        bus = hwdtb_reg_tuple_val_or(tuple, HWDTB_REG_BUS, 0);
    }

    cs_line = object_property_get_uint(OBJECT(dev), "cs", &error_abort);

    if (!hwdtb_node_has_gpio_output(node->parent, cs_namespace, cs_line)) {
        cs_namespace = "cs";
    }

    if (!hwdtb_node_has_gpio_output(node->parent, cs_namespace, cs_line)) {
        hwdtb_report_err(node->parent,
                         "No CS ouput pin found on SSI initiator");
        return;
    }

    if (bus > 0) {
        g_autoptr(GString) str = g_string_new("spi");
        BusState *parent_bus;
        int num_busses = bus;
        int num_cs_lines = 0;

        /*
         * We don't have enough semantic to associate this slave on bus i to the
         * corresponding CS line GPIO index j on the initiator. The specified
         * index in the reg property can be either relative to the SPI bus
         * (Linux DTBs) or absolute (current hwdtbs). Use the following
         * heuristic:
         *    - Try to lookup the number of SSI bus `b' this initiator exposes
         *    - lookup the number of CS lines `l' it has
         *    - Divide l by b to obtain the number of CS lines per bus.
         *    - Apply the offset "CS lines per bus * bus index" to the CS line
         *      specified in the reg property if the bus index is greater than
         *      0 and the CS line lower than the number of CS lines per bus.
         *
         * This assumes a regular repartition of the lines across the busses.
         */
        g_string_append_printf(str, "%d", num_busses);
        parent_bus = qdev_get_child_bus(parent, str->str);

        while (parent_bus) {
            num_busses++;

            g_string_printf(str, "bus%d", num_busses);
            parent_bus = qdev_get_child_bus(parent, str->str);
        }

        /* At this point we know for sure we have at least `bus + 1' busses */
        g_assert(num_busses > bus);

        while (hwdtb_node_has_gpio_output(node->parent, cs_namespace,
                                          num_cs_lines)) {
            num_cs_lines++;
        }

        if (cs_line < (num_cs_lines / num_busses)) {
            cs_line += num_cs_lines / num_busses;
        }
    }

    cs_gpio = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
    qdev_connect_gpio_out_named(parent, cs_namespace, cs_line, cs_gpio);

    out_descr = g_string_new("");
    g_string_append_printf(out_descr, "%s[%d]", cs_namespace ?: "unnamed-gpio",
                           cs_line);

    in_descr = g_string_new(SSI_GPIO_CS);
    g_string_append(in_descr, "[0]");

    trace_hwdtb_node_connect_gpio(node->parent->path,
                                  out_descr->str,
                                  node->path,
                                  in_descr->str);
}

/*
 * SSI target devices must connect to an initiator (an SSI bus) when realized.
 * They are described as follows in hwdtbs:
 *
 * ssi_initiator {
 *      #address-cells = <1>;
 *      #size-cells = <0>;
 *      #bus-cells = <1>;
 *      [...]
 *
 *      ssi_target {
 *          [...]
 *          reg = <1 2>;
 *      };
 * };
 *
 * The initiator is supposed to have a #bus-cells property (the sole use for the
 * bus cell in reg properties). The first reg entry (address cell) on the target
 * is the chip select line index value. The second (bus cell) is the bus index
 * value on the initiator. This translates to bus "spix" with x the index
 * value on the initiator.
 *
 * In this example, the target would connect to bus spi2 using CS pin 1.
 *
 * Note that there is two possible and opposing meanings for the CS line value:
 *    - The one found in Linux DTBs, CS is relative to the bus
 *    - The one found in "recent" hwdtbs (ZynqMP, Versal and later), CS is
 *      absolute, so this is the index of the GPIO on the QEMU model of the
 *      initiator.
 * Supporting both is a nightmare. See the heuristic in
 * ssi_target_connect_cs_gpio
 *
 * Also note that the hwdtb legacy behaviour is to fallback on bus "spi" if the
 * bus "spix" is not found on the initiator.
 */
static BusState *get_bus_for_ssi_target(HwDtbNode *node)
{
    int cs = 0;
    int bus_idx = 0;
    g_autoptr(GString) bus_str = g_string_new("spi");
    BusState *bus;
    DeviceState *dev, *parent;
    HwDtbRegTuple *reg;

    if (!node->parent) {
        return NULL;
    }

    reg = hwdtb_node_reg_get_first(node);

    if (reg == NULL) {
        qemu_log_mask(LOG_FDT, "%s: missing reg property for SSI target. "
                      "Defaulting to bus spi0, cs 0\n", node->path);
    } else {
        if (!reg->entry[HWDTB_REG_ADDR].valid) {
            qemu_log_mask(LOG_FDT, "%s: missing reg address cell for SSI target. "
                          "Defaulting to chip select line 0.\n", node->path);
        } else {
            cs = reg->entry[HWDTB_REG_ADDR].val;
        }

        if (!reg->entry[HWDTB_REG_BUS].valid) {
            qemu_log_mask(LOG_FDT, "%s: missing reg bus cell for SSI target. "
                          "Defaulting to bus spi0.\n", node->path);
        } else {
            bus_idx = reg->entry[HWDTB_REG_BUS].val;
        }
    }

    dev = DEVICE(hwdtb_get_obj(node));
    parent = HWDTB_NODE_AS(node->parent, DEVICE);

    if (parent == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent is not a device. "
                      "Cannot query SSI bus\n", node->path);
        return NULL;
    }

    g_string_append_printf(bus_str, "%d", bus_idx);
    bus = qdev_get_child_bus(parent, bus_str->str);

    if (bus == NULL) {
        /* fallback on bus "spi" */
        bus = qdev_get_child_bus(parent, "spi");
    }

    if (bus == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent has no SSI bus. Tried %s and spi\n",
                      node->path, bus_str->str);
        return NULL;
    }

    trace_hwdtb_node_parent_ssi_target(node->path, node->parent->path,
                                       bus->name, cs);
    qdev_prop_set_uint8(dev, "cs", cs);

    hwdtb_node_register_callback(node, HWDTB_PASS_CONNECT_GPIO,
                                 ssi_target_connect_cs_gpio, NULL);
    return bus;
}

/*
 * i2c targets can be described using two different methods:
 *
 * First method: the target is a direct child of the initiator:
 *
 * initiator {
 *     #address-cells = <1>;
 *     #size-cells = <0>;
 *     [...]
 *
 *     target {
 *         reg = <0x52>;
 *     };
 * };
 *
 * In this example, the target will be mapped on bus "i2c" of the initiator at
 * address 0x52.
 *
 * Second method: the target is under an intermediate empty node:
 *
 * initiator {
 *     #address-cells = <1>;
 *     #size-cells = <0>;
 *     [...]
 *
 *     i2c@0 {
 *         #address-cells = <1>;
 *         #size-cells = <0>;
 *         reg = <0>;
 *
 *         target {
 *             reg = <0x52>;
 *         };
 *     };
 * };
 *
 * This time the target is mapped on bus "i2c@0" of the initiator at address
 * 0x52.
 *
 * Notes:
 *    - The `reg = <0>;' property on the intermediate node seems redundant and
 *      was unused by the legacy fdt_generic code. It is present in all existing
 *      hwdtbs though.
 *    - The legacy fdt_generic code had support only for intermediate nodes
 *      named `i2c@0' to `i2c@7'.
 */
static BusState *get_bus_for_i2c_target(HwDtbNode *node)
{
    BusState *bus;
    const char *bus_str;
    HwDtbNode *parent_node = node;
    DeviceState *parent_dev = NULL;
    size_t i = 0;
    uint8_t addr = 0;
    HwDtbRegTuple *reg;

    /*
     * Try to find the parent device according to method 1 and 2. Go up in the
     * hierarchy of at most two steps.
     */
    while ((parent_dev == NULL) && (i < 2)) {
        if (!parent_node->parent) {
            return NULL;
        }

        if (parent_node->parent && HWDTB_NODE_AS(parent_node->parent, DEVICE)) {
            parent_dev = DEVICE(hwdtb_get_obj(parent_node->parent));
        }

        i++;
        parent_node = parent_node->parent;
    }

    if (parent_dev == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent is not a device. "
                      "Cannot query i2c bus\n", node->path);
        return NULL;
    }

    reg = hwdtb_node_reg_get_first(node);

    if (reg == NULL) {
        qemu_log_mask(LOG_FDT, "%s: missing reg property for i2c target. "
                      "Defaulting to address 0\n", node->path);
    } else {
        if (!reg->entry[HWDTB_REG_ADDR].valid) {
            qemu_log_mask(LOG_FDT, "%s: missing reg address cell for i2c target. "
                          "Defaulting to address 0.\n", node->path);
        } else {
            addr = reg->entry[HWDTB_REG_ADDR].val;
        }
    }

    switch (i) {
    case 1:
        /* method 1: bus name hardcoded to "i2c" */
        bus_str = "i2c";
        break;

    case 2:
        /* method 2: bus name is the intermediate node name */
        bus_str = fdt_get_name(node->hwdtb->fdt, node->parent->offset, NULL);
        break;

    default:
        g_assert_not_reached();
    }

    bus = qdev_get_child_bus(parent_dev, bus_str);

    if (bus == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent has no i2c bus named %s\n",
                      node->path, bus_str);
        return NULL;
    }

    trace_hwdtb_node_parent_i2c_target(node->path, parent_node->path,
                                bus->name, addr);
    qdev_prop_set_uint8(DEVICE(hwdtb_get_obj(node)), "address", addr);

    return bus;
}

static BusState *get_bus_for_i3c_target(HwDtbNode *node)
{
    BusState *bus;
    HwDtbNode *parent_node = node->parent;
    DeviceState *parent_dev = NULL;

    if (parent_node == NULL) {
        return NULL;
    }

    parent_dev = HWDTB_NODE_AS(parent_node, DEVICE);

    if (parent_dev == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent is not a device. "
                      "Cannot query i3c bus\n", node->path);
        return NULL;
    }

    bus = qdev_get_child_bus(parent_dev, parent_dev->id);

    if (bus == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent has no i3c bus named %s\n",
                      node->path, parent_dev->id);
        return NULL;
    }

    trace_hwdtb_node_parent_i3c_target(node->path, parent_node->path,
                                       bus->name);
    return bus;
}

/*
 * PHY nodes are parented to a TYPE_MDIO device. This device exposes a
 * TYPE_MDIO_BUS bus named "mdio-bus".
 *
 * The PHY hwdtb reg property is also a QOM property on PHY devices, so it is
 * already set at this point.
 */
static BusState *get_bus_for_phy(HwDtbNode *node)
{
    BusState *bus;
    DeviceState *parent;
    uint8_t reg;

    if (!node->parent) {
        return NULL;
    }

    parent = HWDTB_NODE_AS(node->parent, DEVICE);

    if (parent == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent is not a device. "
                      "Cannot query MDIO bus\n", node->path);
        return NULL;
    }

    bus = qdev_get_child_bus(parent, "mdio-bus");

    if (bus == NULL) {
        qemu_log_mask(LOG_FDT, "%s: parent has no MDIO bus named `mdio-bus'\n",
                      node->path);
        return NULL;
    }

    /*
     * Retrieve the reg value by reading the device property directly. It has
     * already been set during the property setting phase. If not specified in
     * the hwdtb, we'll get the default value.
     */
    reg = object_property_get_uint(OBJECT(hwdtb_get_obj(node)), "reg",
                                   &error_abort);

    trace_hwdtb_node_parent_mdio_phy(node->path, node->parent->path, reg);
    return bus;
}

static bool ufshc_parent_targeting_dev(HwDtbNode *node, void *opaque)
{
    HwDtbNode *ufs_dev = (HwDtbNode *) opaque;
    uint32_t ufs_target, dev_phandle;

    /* ufshc-sysbus is not correctly split into header/source files */
    if (!object_dynamic_cast(hwdtb_get_obj(node), "ufshc-sysbus")) {
        return false;
    }

    if (!hwdtb_node_get_prop_uint32(node, "ufs-target", &ufs_target)) {
        return false;
    }

    if (!hwdtb_node_get_prop_uint32(ufs_dev, "phandle", &dev_phandle)) {
        return false;
    }

    return ufs_target == dev_phandle;
}

/*
 * -- Legacy --
 * UFS devices need refactoring. We need a proper parenting in the DTB. For now
 * all we have is a ufs-target link property on ufshc-sysbus devices pointing to
 * ufs-dev.
 *
 * Previously the ufshc-sysbus device was parenting the ufs-dev on its bus in
 * its realize function. This does not work because it introduces a realize
 * dependency loop (ufs-dev cannot be parented to a bus before it is realized,
 * but also cannot be realized without a bus).
 *
 * With the introduction of this code, the aforementioned parenting has been
 * removed. The parenting is now done here. Look for a ufshc-sysbus device with
 * a ufs-target pointing to this node. If found, return its internal bus.
 */
static BusState *find_ufshc_bus_for_ufs_dev(HwDtbNode *node)
{
    HwDtbNode *ufshc_parent;

    ufshc_parent = hwdtb_find_node(node->hwdtb,
                                   ufshc_parent_targeting_dev,
                                   node);

    if (ufshc_parent == NULL) {
        hwdtb_report_err(node, "No ufshc-sysbus parent found. Cannot realize "
                               "the device. This is a fatal error.");
        return NULL;
    }

    return qdev_get_child_bus(HWDTB_NODE_AS(ufshc_parent, DEVICE), "ufs-bus.0");
}

static bool node_needs_bus(HwDtbNode *node, const char *bus_type)
{
    DeviceClass *dc;

    /* Here we expect a device */
    dc = DEVICE_GET_CLASS(hwdtb_get_obj(node));

    if (dc->bus_type == NULL) {
        return false;
    }

    return strcmp(dc->bus_type, bus_type) == 0;
}

static BusState *hwdtb_get_bus_for_device(HwDtbNode *node)
{
    g_assert(hwdtb_get_obj(node));

    if (HWDTB_NODE_AS(node, SSI_PERIPHERAL)) {
        return get_bus_for_ssi_target(node);
    }

    if (node_needs_bus(node, TYPE_I2C_BUS)) {
        return get_bus_for_i2c_target(node);
    }

    if (node_needs_bus(node, TYPE_I3C_BUS)) {
        return get_bus_for_i3c_target(node);
    }

    if (node_needs_bus(node, TYPE_MDIO_BUS)) {
        return get_bus_for_phy(node);
    }

    if (node_needs_bus(node, TYPE_SYSTEM_BUS)) {
        return sysbus_get_default();
    }

    if (node_needs_bus(node, TYPE_UFS_BUS)) {
        return find_ufshc_bus_for_ufs_dev(node);
    }

    return NULL;
}

static void hwdtb_dev_reset_handler(void *opaque)
{
    DeviceState *dev = DEVICE(opaque);

    device_cold_reset(dev);
}

static bool bus_required(HwDtbNode *node)
{
    return DEVICE_GET_CLASS(hwdtb_get_obj(node))->bus_type != NULL;
}

static void hwdtb_realize(HwDtbNode *node)
{
    DeviceState *dev;
    BusState *bus;

    if (hwdtb_is_proxy_to_foreign(node)) {
        /* not created by hwdtb, don't try to realize it */
        return;
    }

    dev = HWDTB_NODE_AS(node, DEVICE);

    if (dev == NULL) {
        return;
    }

    bus = hwdtb_get_bus_for_device(node);

    if (bus_required(node) && (bus == NULL)) {
        error_report("hwdtb fatal error: %s: cannot get a bus for the device",
                     node->path);
        abort();
    }

    dev->id = g_strdup(fdt_get_name(node->hwdtb->fdt, node->offset, NULL));

    qdev_realize(dev, bus, &error_abort);
    trace_hwdtb_node_realize(node->path);

    if (!HWDTB_NODE_AS(node, SYS_BUS_DEVICE)) {
        /*
         * A device that is not a sysbus device needs to be reset manually.
         * Register a reset handler here.
         */
        qemu_register_reset(hwdtb_dev_reset_handler, hwdtb_get_obj(node));
    }
}

static void hwdtb_realize_cpus(HwDtbNode *node)
{
    if (object_dynamic_cast(hwdtb_get_obj(node), TYPE_CPU)) {
        hwdtb_realize(node);
    }
}

static void realize_cpu_cluster(gpointer key, gpointer value, gpointer opaque)
{
    Object *obj = (Object *) value;
    DeviceState *cluster = DEVICE(obj);
    HwDtb *hwdtb = (HwDtb *) opaque;
    const char *cpu_type = (const char *) key;
    uint32_t cluster_id;

    cluster_id = hwdtb->next_cluster_id++;
    qdev_prop_set_uint32(cluster, "cluster-id", cluster_id);

    trace_hwdtb_node_realize_auto_cpu_cluster(cpu_type, cluster_id);
    qdev_realize_and_unref(cluster, NULL, &error_abort);
}

/*
 * Realize automatically created CPU clusters.
 */
static void realize_cpu_clusters(HwDtb *hwdtb)
{
    g_hash_table_foreach(hwdtb->cpu_clusters, realize_cpu_cluster, hwdtb);
}

static void hwdtb_realize_others(HwDtbNode *node)
{
    if (!object_dynamic_cast(hwdtb_get_obj(node), TYPE_CPU)) {
        hwdtb_realize(node);
    }
}

void hwdtb_realize_devs(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, hwdtb_realize_cpus);
    realize_cpu_clusters(hwdtb);
    hwdtb_walk(hwdtb, hwdtb_realize_others);
}
