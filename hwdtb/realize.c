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

    if (node_needs_bus(node, TYPE_SYSTEM_BUS)) {
        return sysbus_get_default();
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
