/*
 * HWDTB block device bindings
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "system/blockdev.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "trace.h"

/*
 * Try to find the `blockdev-node-name' property on the node and attach it to
 * the corresponding block device.
 */
static bool node_attach_block_dev_node_name(HwDtbNode *node)
{
    const char *bd_node_name;

    bd_node_name = hwdtb_node_get_prop_string(node, "blockdev-node-name");

    if (bd_node_name == NULL) {
        return false;
    }

    if (bdrv_find_node(bd_node_name) == NULL) {
        /*
         * The blockdev does not exist. Still return true to not try the legacy
         * binding method.
         */
        return true;
    }

    qdev_prop_set_string(DEVICE(hwdtb_get_obj(node)), "drive", bd_node_name);
    trace_hwdtb_node_set_prop_drive(node->path, "drive", bd_node_name);

    return true;
}

static void node_attach_block_dev(HwDtbNode *node)
{
    if (!HWDTB_NODE_AS(node, DEVICE)) {
        return;
    }

    /*
     * Legacy fdt_generic code relies on the existance of a `drive' QOM property
     * on the device to perform block device connection on it. Stick to this
     * behaviour for now. It seems a bit limiting, for example in the case of a
     * device with multiple block device connections.
     */
    if (!object_property_find(hwdtb_get_obj(node), "drive")) {
        return;
    }

    /*
     * Try with the `blockdev_node_name' property. This is normal way of
     * attaching a block device created on the command line using
     * -blockdev node-name=...
     */
    if (node_attach_block_dev_node_name(node)) {
        return;
    }
}

void hwdtb_attach_block_devs(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_attach_block_dev);
}
