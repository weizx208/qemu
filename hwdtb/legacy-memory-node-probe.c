/*
 * HWDTB memory node discovery
 *
 * This pass walks the tree to find the first memory node and sum up the RAM
 * amount under it. This amount is used later when creating memory-region-spec
 * nodes to create the right total amount of RAM in the machine.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qemu/cutils.h"

static HwDtbNode *find_first_mem_node(HwDtbNode *root)
{
    HwDtbNode *child;

    /*
     * We are looking for the first /memory@xxx node, so at depth 1 in the tree.
     */
    hwdtb_node_foreach_child(child, root) {
        if (strstart(child->path, "/memory", NULL)) {
            return child;
        }
    }

    return NULL;
}

static bool node_is_a_memory_region(HwDtbNode *node)
{
    const char *compat = hwdtb_node_get_prop_string(node, "compatible");

    /* We are before creation pass, we cannot rely on the QOM objects yet. */

    if (compat == NULL) {
        return false;
    }

    return !strcmp(compat, "qemu:memory-region");
}

static uint64_t get_node_ram_amount(HwDtbNode *node, uint32_t mem_node_phandle)
{
    uint32_t parent;
    HwDtbRegTuple *tuple;

    if (!node_is_a_memory_region(node)) {
        return 0;
    }

    /*
     * The legacy code only check memory regions with a container property.
     * mem_node child nodes or reg-extended property are not taken into account.
     */
    if (!hwdtb_node_get_prop_uint32(node, "container", &parent)) {
        return 0;
    }

    if (parent != mem_node_phandle) {
        return 0;
    }

    if (QSIMPLEQ_EMPTY(&node->reg)) {
        return 0;
    }

    tuple = QSIMPLEQ_FIRST(&node->reg);

    if (!tuple->entry[HWDTB_REG_SIZE].valid) {
        return 0;
    }

    return tuple->entry[HWDTB_REG_SIZE].val;
}

/*
 * Walk the @node and its children and try to find memory nodes having
 * mem_node_phandle as a container.
 *
 * @return the sum of the size of all matching nodes
 */
static uint64_t node_probe_ram_amount(HwDtbNode *node,
                                      uint32_t mem_node_phandle)
{
    HwDtbNode *child;
    uint64_t ram_amount;

    ram_amount = get_node_ram_amount(node, mem_node_phandle);

    hwdtb_node_foreach_child(child, node) {
        ram_amount += node_probe_ram_amount(child, mem_node_phandle);
    }

    return ram_amount;
}

void hwdtb_legacy_memory_node_probe(HwDtb *hwdtb)
{
    HwDtbNode *mem_node = find_first_mem_node(hwdtb->root);
    uint32_t mem_node_phandle;

    if (mem_node == NULL) {
        return;
    }

    hwdtb->first_mem_node = mem_node;

    if (!hwdtb_node_get_prop_uint32(mem_node, "phandle", &mem_node_phandle)) {
        return;
    }

    hwdtb->fulfilled_ram_amount = node_probe_ram_amount(hwdtb->root,
                                                        mem_node_phandle);
}
