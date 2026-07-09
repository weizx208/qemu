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
#include "hw/ssi/ssi.h"
#include "hw/nvram/xlnx-bbram.h"
#include "hw/nvram/xlnx-efuse.h"
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

/*
 * -- Legacy --
 * Legacy heuristic assiociating a device type to a block interface type.
 */
static BlockInterfaceType node_get_block_iface_type(HwDtbNode *node)
{
    if (HWDTB_NODE_AS(node, XLNX_BBRAM)) {
        return IF_PFLASH;
    }

    if (HWDTB_NODE_AS(node, XLNX_EFUSE)) {
        return IF_PFLASH;
    }

    if (HWDTB_NODE_AS(node, SSI_PERIPHERAL)) {
        return IF_MTD;
    }

    /* No match */
    return IF_NONE;
}

/*
 * -- Legacy --
 * Legacy heuristic associating a device type to a default drive index.
 *
 * Some dtbs don't have the drive-index property. The legacy code uses some
 * tricks by reading device properties to deduce the machine type, and then
 * attach the drive at correct index to the device.
 *
 * Those are reproduced here for backward compatibility.
 */
static uint32_t node_get_block_default_drive_idx(HwDtbNode *node)
{
    if (HWDTB_NODE_AS(node, XLNX_BBRAM)) {
        bool is_versal;

        /* not versal -> zynqmp */
        is_versal = object_property_get_uint(hwdtb_get_obj(node), "crc-zpads",
                                             &error_abort) == 0;

        return is_versal ? 0 : 2;
    }

    if (HWDTB_NODE_AS(node, XLNX_EFUSE)) {
        bool is_versal;

        /* not versal -> zynqmp */
        is_versal = object_property_get_uint(hwdtb_get_obj(node), "efuse-size",
                                             &error_abort) > 2048;

        return is_versal ? 1 : 3;
    }

    return UINT32_MAX;
}

static DriveInfo *node_get_drive_info(HwDtbNode *node, BlockInterfaceType type)
{
    uint32_t index;

    if (hwdtb_node_get_prop_uint32(node, "drive-index", &index)) {
        return drive_get_by_index(type, index);
    } else {
        return drive_get_next(type);
    }
}

static bool node_attach_interface_index(HwDtbNode *node)
{
    BlockInterfaceType iface;
    uint32_t default_idx;
    DriveInfo *info;
    BlockBackend *bb;

    iface = node_get_block_iface_type(node);

    if (iface == IF_NONE) {
        return false;
    }

    default_idx = node_get_block_default_drive_idx(node);

    if (default_idx != UINT32_MAX) {
        info = drive_get_by_index(iface, default_idx);
    } else {
        info = node_get_drive_info(node, iface);
    }

    if (info == NULL) {
        return false;
    }

    bb = blk_by_legacy_dinfo(info);
    qdev_prop_set_drive(DEVICE(hwdtb_get_obj(node)), "drive", bb);
    trace_hwdtb_node_set_prop_drive(node->path, "drive", blk_name(bb));

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

    /*
     * -- Legacy --
     * Try the legacy BlockInterfaceType/index method.
     */
    node_attach_interface_index(node);
}

void hwdtb_attach_block_devs(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_attach_block_dev);
}
