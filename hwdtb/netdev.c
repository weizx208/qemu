/*
 * HWDTB network device bindings
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qom/object.h"
#include "net/net.h"
#include "trace.h"

static void node_attach_net_dev(HwDtbNode *node)
{
    Object *obj = hwdtb_get_obj(node);
    DeviceState *dev;
    NICInfo *nic_info;

    dev = HWDTB_NODE_AS(node, DEVICE);

    if (dev == NULL) {
        return;
    }

    /*
     * Devices using DEFINE_NIC_PROPERTIES macro in their property array end up
     * with the following two properties.
     */
    if (!object_property_find(obj, "mac")) {
        return;
    }

    if (!object_property_find(obj, "netdev")) {
        return;
    }

    nic_info = qemu_find_nic_info(object_get_typename(obj), true, NULL);

    if (nic_info == NULL) {
        return;
    }

    qdev_set_nic_properties(dev, nic_info);
    trace_hwdtb_node_set_prop_nic(node->path, nic_info->name);
}

void hwdtb_attach_net_devs(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_attach_net_dev);
}
