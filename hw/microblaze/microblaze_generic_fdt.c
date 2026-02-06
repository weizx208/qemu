/*
 * Model of Petalogix linux reference design for all boards
 *
 * Copyright (c) 2009 Edgar E. Iglesias.
 * Copyright (c) 2009 Michal Simek.
 * Copyright (c) 2012 Peter A.G. Crosthwaite (peter.croshtwaite@petalogix.com)
 * Copyright (c) 2012 Petalogix Pty Ltd.
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hw/hw.h"
#include "qemu/log.h"
#include "system/reset.h"
#include "hw/boards.h"
#include "cpu.h"
#include "system/device_tree.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "qapi/error.h"
#include "system/qtest.h"
#include "migration/vmstate.h"

#include "hw/fdt_generic_util.h"
#include "qemu/hwdtb.h"

#include "boot.h"

#include <libfdt.h>

#define IS_PETALINUX_MACHINE \
    (!strcmp(MACHINE_GET_CLASS(machine)->name, MACHINE_NAME "-plnx"))

#define QTEST_RUNNING (qtest_enabled() && qtest_driver())

#define LMB_BRAM_SIZE  (128 * 1024)

#define MACHINE_NAME "microblaze-fdt"

#if TARGET_BIG_ENDIAN
int endian = 1;
#else
int endian;
#endif

static void
microblaze_generic_fdt_init(MachineState *machine)
{
    uint64_t ram_kernel_base = 0, ram_kernel_size = 0;
    void *hw_fdt = NULL, *fdt = NULL;
    const char *dtb_arg, *hw_dtb_arg;
    const char *kernel_filename;
    int fdt_size;
    HwDtb *hwdtb;

    /* for memory node */
    HwDtbNode *mem_node;
    char node_path[DT_PATH_LENGTH];
    MemoryRegion *main_mem;

    /* For DMA node */
    char dma_path[DT_PATH_LENGTH] = { 0 };
    uint32_t memory_phandle;

    /* For Ethernet nodes */
    char **eth_paths;
    char *phy_path;
    char *mdio_path;
    uint32_t n_eth;
    uint32_t prop_val;

    dtb_arg = machine->dtb;
    hw_dtb_arg = machine->hw_dtb;
    if (!dtb_arg && !hw_dtb_arg) {
        goto no_dtb_arg;
    }

    /* If the user only provided a -dtb, use it as the hw description.  */
    if (!hw_dtb_arg) {
        hw_dtb_arg = dtb_arg;
    }

    /* If the user only provided a -hw-dtb, use it as the dtb.  */
    if (!dtb_arg) {
        dtb_arg = hw_dtb_arg;
    }

    fdt = load_device_tree(dtb_arg, &fdt_size);
    if (!fdt) {
        hw_error("Error: Unable to load Device Tree %s\n", dtb_arg);
        return;
    }

    hw_fdt = load_device_tree(hw_dtb_arg, NULL);
    if (!hw_fdt) {
        hw_error("Error: Unable to load Device Tree %s\n", hw_dtb_arg);
        return;
    }

    if (IS_PETALINUX_MACHINE) {
        int offset = 0;

        /* Change simple-bus compatibles to qemu:system-memory */
        while ((offset = fdt_next_node(hw_fdt, offset, NULL))) {
            const struct fdt_property *prop;

            if ((prop = fdt_get_property(hw_fdt, offset, "compatible", NULL))) {
                const char *compat = prop->data;

                if (!strcmp(compat, "simple-bus")) {
                    fdt_setprop_string(hw_fdt, offset, "compatible",
                                       "qemu:system-memory");
                    offset = 0;
                }
            }
        }

        /* Remove interrupt-names properties */
        while ((offset = fdt_next_node(hw_fdt, offset, NULL))) {
            if ((fdt_get_property(hw_fdt, offset, "interrupt-names", NULL))) {
                fdt_delprop(hw_fdt, offset, "interrupt-names");
                offset = 0;
            }
        }
    }

    /* find memory node or add new one if needed */
    while (qemu_devtree_get_node_by_name(hw_fdt, node_path, "memory")) {
        qemu_fdt_add_subnode(hw_fdt, "/memory@0");
        qemu_fdt_setprop_cells(hw_fdt, "/memory@0", "reg", 0, machine->ram_size);
    }

    if (!qemu_fdt_getprop(hw_fdt, "/memory", "compatible", NULL, 0, NULL)) {
        qemu_fdt_setprop_string(hw_fdt, "/memory", "compatible",
                                "qemu:memory-region");
        qemu_fdt_setprop_cells(hw_fdt, "/memory", "qemu,ram", 1);
    }

    if (IS_PETALINUX_MACHINE) {
        /* If using a *-plnx machine, the AXI DMA memory links are not included
         * in the DTB by default. To avoid seg faults, add the links in here if
         * they have not already been added by the user
         */
        qemu_devtree_get_node_by_name(hw_fdt, dma_path, "dma");

        if (strcmp(dma_path, "") != 0) {
            memory_phandle = qemu_fdt_check_phandle(hw_fdt, node_path);

            if (!memory_phandle) {
                memory_phandle = qemu_fdt_alloc_phandle(hw_fdt);

                qemu_fdt_setprop_cells(hw_fdt, "/memory", "linux,phandle",
                                       memory_phandle);
                qemu_fdt_setprop_cells(hw_fdt, "/memory", "phandle",
                                       memory_phandle);
            }

            if (!qemu_fdt_getprop(hw_fdt, dma_path, "sg", NULL, 0, NULL)) {
                qemu_fdt_setprop_phandle(hw_fdt, dma_path, "sg", node_path);
            }

            if (!qemu_fdt_getprop(hw_fdt, dma_path, "s2mm", NULL, 0, NULL)) {
                qemu_fdt_setprop_phandle(hw_fdt, dma_path, "s2mm", node_path);
            }

            if (!qemu_fdt_getprop(hw_fdt, dma_path, "mm2s", NULL, 0, NULL)) {
                qemu_fdt_setprop_phandle(hw_fdt, dma_path, "mm2s", node_path);
            }
        }

        /* Copy phyaddr value from phy node reg property */
        n_eth = qemu_devtree_get_n_nodes_by_name(hw_fdt, &eth_paths, "ethernet");

        while (n_eth--) {
            mdio_path = qemu_devtree_get_child_by_name(hw_fdt, eth_paths[n_eth],
                                                       "mdio");
            if (mdio_path) {
                phy_path = qemu_devtree_get_child_by_name(hw_fdt, mdio_path,
                                                          "phy");
                if (phy_path) {
                    prop_val = qemu_fdt_getprop_cell(hw_fdt, phy_path, "reg", 0,
                                                     NULL, &error_abort);
                    qemu_fdt_setprop_cell(hw_fdt, eth_paths[n_eth], "xlnx,phyaddr",
                                          prop_val);
                    g_free(phy_path);
                } else {
                    qemu_log_mask(LOG_GUEST_ERROR, "phy not found in %s",
                                  mdio_path);
                }
                g_free(mdio_path);
            }
            g_free(eth_paths[n_eth]);
        }
        g_free(eth_paths);
    }

    /* Instantiate peripherals from the FDT.  */
    hwdtb = hwdtb_create_machine(machine, hw_fdt);
    mem_node = hwdtb_get_node_by_path(hwdtb, node_path);
    main_mem = HWDTB_NODE_AS(mem_node, MEMORY_REGION);

    if (main_mem && !memory_region_is_mapped(main_mem)) {
        /* If the memory region is not mapped, map it here.
         * It has to be mapped somewhere, so guess that the base address
         * is where the kernel starts
         */

        if (hwdtb_node_reg_get_first_addr(mem_node, &ram_kernel_base)
            && hwdtb_node_reg_get_first_size(mem_node, &ram_kernel_size)) {
            memory_region_add_subregion(get_system_memory(), ram_kernel_base,
                                        main_mem);

            if (ram_kernel_base && IS_PETALINUX_MACHINE) {
                /* If the memory added is at an offset from zero QEMU will error
                 * when an ISR/exception is triggered. Add a small amount of hack
                 * RAM to handle this.
                 */
                MemoryRegion *hack_ram = g_new(MemoryRegion, 1);

                memory_region_init_ram_nomigrate(hack_ram, NULL, "hack_ram",
                                                 0x1000, &error_abort);
                vmstate_register_ram_global(hack_ram);
                memory_region_add_subregion(get_system_memory(), 0, hack_ram);
            }
        }
    } else if (main_mem) {
        ram_kernel_base = object_property_get_int(OBJECT(main_mem), "addr", NULL);
        ram_kernel_size = object_property_get_int(OBJECT(main_mem), "size", NULL);
    } else {
        ram_kernel_base = 0;
        ram_kernel_size = 0;
    }

    hwdtb_free(hwdtb);

    kernel_filename = machine->kernel_filename;
    if (kernel_filename) {
        microblaze_load_kernel(MICROBLAZE_CPU(first_cpu), true, ram_kernel_base,
                               ram_kernel_size, machine->initrd_filename, NULL,
                               NULL, fdt, fdt_size);
    }

    return;
no_dtb_arg:
    if (!QTEST_RUNNING) {
        hw_error("DTB must be specified for %s machine model\n", MACHINE_NAME);
    }
    return;
}

static void microblaze_generic_fdt_machine_init(MachineClass *mc)
{
    mc->desc = "Microblaze device tree driven machine model";
    mc->init = microblaze_generic_fdt_init;
    mc->default_cpus = 8;
}

static void microblaze_generic_fdt_plnx_machine_init(MachineClass *mc)
{
    mc->desc = "Microblaze device tree driven machine model for PetaLinux";
    mc->init = microblaze_generic_fdt_init;
}

DEFINE_MACHINE(MACHINE_NAME, microblaze_generic_fdt_machine_init)
DEFINE_MACHINE(MACHINE_NAME "-plnx", microblaze_generic_fdt_plnx_machine_init)
