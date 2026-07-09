/*
 * HWDTB legacy mmap interface call
 *
 * Call the legacy FDT_GENERIC_MMAP interface on devices that need so.
 * Those devices should eventually be fixed and this interface completely
 * removed.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "hw/fdt_generic_util.h"
#include "trace.h"

static void hwdtb_reg_to_fdt_generic_reg_prop_info(HwDtbNode *node,
                                                   FDTGenericRegPropInfo *ret)
{
    size_t i, num_tuple = 0;
    HwDtbRegTuple *tuple;

    hwdtb_node_foreach_reg_tuple(tuple, node) {
        num_tuple++;
    }

    ret->n = num_tuple;
    ret->parents = g_new(Object *, num_tuple);

    for (i = 0; i < FDT_GENERIC_REG_TUPLE_LENGTH; i++) {
        ret->x[i] = g_new(uint64_t, num_tuple);
    }

    hwdtb_node_foreach_reg_tuple(tuple, node) {
        HwDtbNode *parent;

        parent = tuple->extended ?
                     tuple->target :
                     node->parent;
        ret->parents[tuple->idx] = hwdtb_get_obj(parent);

        for (i = 0; i < FDT_GENERIC_REG_TUPLE_LENGTH; i++) {
            ret->x[i][tuple->idx] = tuple->entry[i].val;
        }
    }
}

static void hwdtb_free_fdt_generic_reg_prop_info(FDTGenericRegPropInfo *info)
{
    size_t i;

    for (i = 0; i < FDT_GENERIC_REG_TUPLE_LENGTH; i++) {
        g_free(info->x[i]);
    }

    g_free(info->parents);
}

static void node_call_legacy_mmap_iface(HwDtbNode *node)
{
    FDTGenericMMap *iface;
    FDTGenericMMapClass *fgmc;
    FDTGenericRegPropInfo info;

    if (!HWDTB_NODE_AS(node, FDT_GENERIC_MMAP)) {
        return;
    }

    iface = FDT_GENERIC_MMAP(hwdtb_get_obj(node));
    fgmc = FDT_GENERIC_MMAP_GET_CLASS(iface);

    if (!fgmc->parse_reg) {
        return;
    }

    hwdtb_reg_to_fdt_generic_reg_prop_info(node, &info);

    fgmc->parse_reg(iface, info, NULL);

    hwdtb_free_fdt_generic_reg_prop_info(&info);
}

void hwdtb_legacy_mmap_iface(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_call_legacy_mmap_iface);
}
