/*
 * HWDTB memory mapping creation
 *
 * This unit takes care of mapping sysbus devices and memory regions onto other
 * memory regions according to their reg or reg-exteneded property.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "hw/sysbus.h"
#include "error.h"
#include "trace.h"

static HwDtbNode *get_parent_mr(HwDtbNode *node, HwDtbRegTuple *tuple,
                                bool report_err)
{
    HwDtbNode *parent;

    if (tuple->extended) {
        parent = tuple->target;

        if (parent == NULL) {
            /* Phandle resolution error already emited by the parser */
            return NULL;
        }

        if (!HWDTB_NODE_AS(parent, MEMORY_REGION)) {
            if (report_err) {
                hwdtb_report_err(node, HWDTB_ERR3(CONN, PARENT, TYPE_MISMATCH),
                                 "reg-extended", parent->path,
                                 TYPE_MEMORY_REGION);
            }
            return NULL;
        }
    } else {
        parent = node->parent;

        if (!HWDTB_NODE_AS(parent, MEMORY_REGION)) {
            if (report_err) {
                hwdtb_report_err(node, HWDTB_ERR2(PARENT, TYPE_MISMATCH),
                                 parent->path, TYPE_MEMORY_REGION);
            }
            return NULL;
        }
    }

    return parent;
}

static bool reg_tuple_get_mapping_info(HwDtbNode *node,
                                       HwDtbRegTuple *tuple,
                                       HwDtbNode **parent,
                                       uint64_t *addr, int *prio)
{
    if (!tuple->entry[HWDTB_REG_ADDR].valid) {
        hwdtb_report_err(node, "missing address in the reg property "
                         "tuple at index %zu", tuple->idx);
        return false;
    }

    *addr = hwdtb_reg_tuple_val_nofail(tuple, HWDTB_REG_ADDR);
    *prio = hwdtb_reg_tuple_val_or(tuple, HWDTB_REG_PRIO, 0);
    *parent = get_parent_mr(node, tuple, true);

    return *parent != NULL;
}

static void sysbus_device_map_tuple(HwDtbNode *node, HwDtbRegTuple *tuple)
{
    HwDtbNode *parent;
    SysBusDevice *sbd;
    MemoryRegion *target;
    MemoryRegion *sub_region;
    uint64_t addr;
    int prio;

    if (!reg_tuple_get_mapping_info(node, tuple, &parent, &addr, &prio)) {
        return;
    }

    sbd = SYS_BUS_DEVICE(hwdtb_get_obj(node));
    sub_region = sysbus_mmio_get_region(sbd, tuple->idx);

    if (sub_region == NULL) {
        hwdtb_report_err(node, "cannot find sysbus device memory region "
                         "at index %zu", tuple->idx);
        return;
    }

    target = MEMORY_REGION(hwdtb_get_obj(parent));

    trace_hwdtb_node_sysbus_dev_map(node->path, tuple->idx, parent->path, addr);
    memory_region_add_subregion_overlap(target, addr, sub_region, prio);
}

static void sysbus_device_map(HwDtbNode *node)
{
    HwDtbRegTuple *tuple;

    hwdtb_node_foreach_reg_tuple(tuple, node) {
        sysbus_device_map_tuple(node, tuple);
    }
}

static void hwdtb_memory_region_map(HwDtbNode *node)
{
    HwDtbRegTuple *tuple = NULL;
    HwDtbNode *parent = NULL;
    MemoryRegion *parent_mr;
    MemoryRegion *mr;
    uint64_t addr;
    int prio;

    if (!QSIMPLEQ_EMPTY(&node->reg)) {
        tuple = QSIMPLEQ_FIRST(&node->reg);
    }

    addr = hwdtb_reg_tuple_val_or_prop_or(node, "addr",
                                          tuple, HWDTB_REG_ADDR, 0);
    prio = hwdtb_reg_tuple_val_or_prop_or(node, "priority",
                                          tuple, HWDTB_REG_PRIO, 0);

    if (tuple != NULL) {
        /*
         * Get parent from reg-extended, or the direct parent when we have a reg
         * property.
         */
        parent = get_parent_mr(node, tuple, false);
    }

    if (!parent && hwdtb_node_has_prop(node, "container")) {
        uint32_t phandle;

        /*
         * -- Legacy --
         * If no reg or reg-extended, try the legacy container property. This
         * can be deprecated in favor of the reg-extended property.
         */

        if (!hwdtb_node_get_prop_uint32(node, "container", &phandle)) {
            hwdtb_report_err(node, HWDTB_ERR_INVAL_PROP, "container");
            return;
        }

        parent = hwdtb_get_node_by_phandle(node->hwdtb, phandle);

        if (parent == NULL) {
            hwdtb_report_err(node, HWDTB_ERR_PHANDLE_NOT_FOUND, "container",
                             phandle);
            return;
        }
    }

    if (parent == NULL) {
        /* no reg tuple, no container property. Skip memory region mapping */
        return;
    }

    mr = MEMORY_REGION(hwdtb_get_obj(node));
    parent_mr = MEMORY_REGION(hwdtb_get_obj(parent));

    trace_hwdtb_node_memory_region_map(node->path, parent->path, addr);
    memory_region_add_subregion_overlap(parent_mr, addr, mr, prio);
}

static void hwdtb_mem_map_node(HwDtbNode *node)
{
    if (HWDTB_NODE_AS(node, SYS_BUS_DEVICE)) {
        sysbus_device_map(node);
    }

    if (HWDTB_NODE_AS(node, MEMORY_REGION)) {
        hwdtb_memory_region_map(node);
    }
}

void hwdtb_mem_map_nodes(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, hwdtb_mem_map_node);
}
