/*
 * HWDTB clock connections
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "hw/qdev-core.h"
#include "hw/qdev-clock.h"
#include "error.h"
#include "trace.h"

static Clock *get_src_clock(HwDtbConnectionTarget *target)
{
    Clock *ret;
    DeviceState *dev = HWDTB_NODE_AS(target->target, DEVICE);

    if (dev) {
        if (target->name == NULL) {
            hwdtb_report_err(target->target, HWDTB_ERR_MISSING_PROP,
                             "output-clock-names");
            return NULL;
        }

        ret = qdev_get_clock_out(dev, target->name);

        if (ret == NULL) {
            hwdtb_report_err(target->target, HWDTB_ERR_CLOCK_OUTPUT_NOT_FOUND,
                             target->name);
        }

        return ret;
    }

    ret = HWDTB_NODE_AS(target->target, CLOCK);

    if (ret) {
        return ret;
    }

    hwdtb_report_err(target->target, HWDTB_ERR_TYPE_MISMATCH_2, TYPE_DEVICE,
                     TYPE_CLOCK);
    return NULL;
}

static void node_connect_clock(HwDtbNode *node, HwDtbConnection *conn)
{
    HwDtbConnectionTarget *target;
    DeviceState *dst_dev;
    Clock *src_clk;

    g_assert(conn->targets->len == 1);
    target = &g_array_index(conn->targets, HwDtbConnectionTarget, 0);

    dst_dev = HWDTB_NODE_AS(node, DEVICE);
    if (dst_dev == NULL) {
        return;
    }

    if (conn->name == NULL) {
        hwdtb_report_err(node, HWDTB_ERR_MISSING_PROP, "clock-names");
        return;
    }

    if (qdev_get_clock_in(dst_dev, conn->name) == NULL) {
        hwdtb_report_err(node, HWDTB_ERR_CLOCK_INPUT_NOT_FOUND, conn->name);
        return;
    }

    src_clk = get_src_clock(target);

    if (src_clk == NULL) {
        return;
    }

    qdev_connect_clock_in(dst_dev, conn->name, src_clk);
    trace_hwdtb_node_clock_connect(node->path, conn->name, target->target->path,
                                   target->name);
}

static void node_connect_clocks(HwDtbNode *node)
{
    HwDtbConnection *conn;

    hwdtb_node_foreach_connection(conn, node, HWDTB_CON_CLOCK) {
        node_connect_clock(node, conn);
    }
}

void hwdtb_connect_clocks(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_connect_clocks);
}

