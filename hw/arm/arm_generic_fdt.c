/*
 * Xilinx Zynq Baseboard System emulation.
 *
 * Copyright (c) 2012 Xilinx. Inc
 * Copyright (c) 2012 Peter A.G. Crosthwaite (peter.crosthwaite@xilinx.com)
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "hw/block/flash.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/config-file.h"
#include "qemu/option.h"
#include "system/system.h"
#include "system/qtest.h"
#include "hw/arm/xlnx-zynqmp.h"
#include "hw/arm/boot.h"
#include "exec/tswap.h"
#include "hw/arm/machines-qom.h"
#include "qemu/hwdtb.h"

#include <libfdt.h>
#include "system/device_tree.h"
#include "hw/hotplug.h"
#include "hw/misc/amd-ddr-memory.h"

#ifndef ARM_GENERIC_FDT_DEBUG
#define ARM_GENERIC_FDT_DEBUG 3
#endif
#define DB_PRINT(lvl, ...) do { \
    if (ARM_GENERIC_FDT_DEBUG > (lvl)) { \
        qemu_log_mask(LOG_FDT, ": %s: ", __func__); \
        qemu_log_mask(LOG_FDT, ## __VA_ARGS__); \
    } \
} while (0);

#define DB_PRINT_RAW(lvl, ...) do { \
    if (ARM_GENERIC_FDT_DEBUG > (lvl)) { \
        qemu_log_mask(LOG_FDT, ## __VA_ARGS__); \
    } \
} while (0);

#define GENERAL_MACHINE_NAME "arm-generic-fdt"
#define ZYNQ7000_MACHINE_NAME "arm-generic-fdt-7series"
#define DEP_GENERAL_MACHINE_NAME "arm-generic-fdt-plnx"

#define QTEST_RUNNING (qtest_enabled() && qtest_driver())

#define ZYNQ7000_MPCORE_PERIPHBASE 0xF8F00000
#define ZYNQ7000_MPCORE_REV 0x413fc090

#define SMP_BOOT_ADDR 0xfffffff0
/* Meaningless, but keeps arm boot happy */
#define SMP_BOOTREG_ADDR 0xfffffffc

#define DDR_LOW_SIZE 0x80000000

static struct arm_boot_info arm_generic_fdt_binfo = {};

/* Entry point for secondary CPU */
static uint32_t zynq_smpboot[] = {
    0xe320f003, /* wfi */
    0xeafffffd, /* beq     <wfi> */
};

typedef struct {
    ram_addr_t ram_kernel_base;
    ram_addr_t ram_kernel_size;
} memory_info;

static void arm_write_secondary_boot(ARMCPU *cpu,
                                      const struct arm_boot_info *info)
{
    int n;

    for (n = 0; n < ARRAY_SIZE(zynq_smpboot); n++) {
        zynq_smpboot[n] = tswap32(zynq_smpboot[n]);
    }
    rom_add_blob_fixed("smpboot", zynq_smpboot, sizeof(zynq_smpboot),
                       SMP_BOOT_ADDR);
}

static void replace_compatible(void *fdt, const char *old, const char *new)
{
    int offset;

    offset = fdt_node_offset_by_compatible(fdt, -1, old);

    while (offset != -FDT_ERR_NOTFOUND) {
        fdt_setprop_string(fdt, offset, "compatible", new);
        offset = fdt_node_offset_by_compatible(fdt, -1, old);
    }
}

static void zynq7000_usb_nuke_phy(void *fdt)
{
    char usb_node_path[DT_PATH_LENGTH];

    int ret = qemu_devtree_node_by_compatible(fdt, usb_node_path,
                                              "xlnx,ps7-usb-1.00.a");
    if (!ret) {
        qemu_fdt_setprop_string(fdt, usb_node_path, "dr_mode", "host");
    }
}

#define ZYNQ7000_QSPI_DUMMY_NAME "/ps7-qspi-dummy@0"

static char *zynq7000_qspi_flash_node_clone(void *fdt)
{
    char qspi_node_path[DT_PATH_LENGTH];
    char qspi_new_node_path[DT_PATH_LENGTH + sizeof(ZYNQ7000_QSPI_DUMMY_NAME)];
    char *qspi_clone_name = NULL;
    uint32_t val[2];

    /* clear node paths */
    memset(qspi_node_path, 0, sizeof(qspi_node_path));
    memset(qspi_new_node_path, 0, sizeof(qspi_new_node_path));

    /* search for ps7 qspi node */
    int ret = qemu_devtree_node_by_compatible(fdt, qspi_node_path,
                                              "xlnx,zynq-qspi-1.0");
    if (ret == 0) {
        int qspi_is_dual = qemu_fdt_getprop_cell(fdt, qspi_node_path,
                                                 "is-dual", 0, false, NULL);
        /* Set bus-cells property to 1 */
        val[0] = cpu_to_be32(1);
        val[1] = 0;
        fdt_setprop(fdt, fdt_path_offset(fdt, qspi_node_path),
                    "#bus-cells", val, 4);

        /* Generate dummy name */
        snprintf(qspi_new_node_path, sizeof(qspi_new_node_path),
                 "%s" ZYNQ7000_QSPI_DUMMY_NAME, qspi_node_path);
        if (strlen(qspi_new_node_path) > DT_PATH_LENGTH) {
            error_report("%s, %zd chars too long DT path!", qspi_new_node_path,
                         strlen(qspi_new_node_path));
        }

        /* get the spi flash node to clone from (assume first child node) */
        int child_num = qemu_devtree_get_num_children(fdt, qspi_node_path, 1);
        char **child_flash = qemu_devtree_get_children(fdt, qspi_node_path, 1);
        if (child_num > 0) {
            char *compat_str = NULL;
            compat_str = qemu_fdt_getprop(fdt, child_flash[0],
                                          "compatible", NULL, false, NULL);
            int lenp = 0;
            void *pm = qemu_fdt_getprop(fdt, child_flash[0],
                                        "parallel-memories", &lenp, 0, NULL);
            if (pm && lenp) {
                qspi_is_dual = 1;
                g_free(pm);
            }
        /* Attach Default flash node to bus 1 */
        val[0] = 0;
        val[1] = 0;
        fdt_setprop(fdt, fdt_path_offset(fdt, child_flash[0]), "reg", val, 8);

            /* Create the cloned node if the qspi controller is in dual spi mode
             * and the compatible string is avaliable */
            if (compat_str != NULL) {
                if (qspi_is_dual == 1) {
                    /* Clone first node, preserving only 'compatible' value */
                    qemu_fdt_add_subnode(fdt, qspi_new_node_path);
                    qemu_fdt_setprop_string(fdt, qspi_new_node_path,
                                             "compatible", compat_str);
                    qspi_clone_name = g_strdup(qspi_new_node_path);

                    /* Attach Dummy flash node to bus 0 */
                    val[0] = cpu_to_be32(1);
                    val[1] = cpu_to_be32(1);
                    fdt_setprop(fdt, fdt_path_offset(fdt, qspi_new_node_path),
                                "reg", val, 8);
                }
                g_free(compat_str);
            }
        }
        g_free(child_flash);
    }

    return qspi_clone_name;
}

static memory_info init_machine(MachineState *machine,
                                void *fdt, ram_addr_t ram_size, bool zynq_7000)
{
    bool dynamic_mem;
    char node_path[DT_PATH_LENGTH];
    memory_info kernel_info;
    Error *errp = NULL;
    uint64_t kernel_base;
    HwDtb *hwdtb;

    if (zynq_7000) {
        if (!qemu_devtree_node_by_compatible(fdt, node_path,
                                             "arm,cortex-a9-gic")) {
            qemu_fdt_setprop_cell(fdt, node_path, "num-priority-bits", 5);
        } else {
            warn_report("%s(): Unable to find GIC", __func__);
        }
    }

    /* Find a memory node or add new one if needed */
    while (qemu_devtree_get_node_by_name(fdt, node_path, "memory")) {
        qemu_fdt_add_subnode(fdt, "/memory@0");
        qemu_fdt_setprop_cells(fdt, "/memory@0", "reg", 0, ram_size);
    }

    if (!qemu_fdt_getprop(fdt, "/memory", "compatible", NULL, 0, NULL)) {
        qemu_fdt_setprop_string(fdt, "/memory", "compatible",
                                "qemu:memory-region");
        qemu_fdt_setprop_cells(fdt, "/memory", "qemu,ram", 1);
    }

    dynamic_mem = object_property_get_bool(OBJECT(qdev_get_machine()),
                                           "dynamic-mem", NULL);
    if (dynamic_mem == false) {
        replace_compatible(fdt, "qemu:memory-region-ddr", "qemu:memory-region");
    } else {
        /* Hide memory-region-spec nodes so that hwdtb does not populate them */
        replace_compatible(fdt, "qemu:memory-region-spec",
                           "qemu:memory-region-spec-disabled");
    }

    /* Instantiate peripherals from the FDT.  */
    hwdtb = hwdtb_create_machine(machine, fdt);

    if (hwdtb_node_get_prop_uint64(hwdtb->root, "kernel-base",
                                   &kernel_base)) {
        kernel_info.ram_kernel_base = kernel_base;
        kernel_info.ram_kernel_size = ram_size;
    } else if (hwdtb->first_mem_node) {
        Object *first_mem_node = hwdtb_get_obj(hwdtb->first_mem_node);
        MemoryRegion *mr = MEMORY_REGION(first_mem_node);

        kernel_info.ram_kernel_base = object_property_get_int(first_mem_node,
                                                              "addr", &errp);
        if (errp) {
            return kernel_info;
        }

        kernel_info.ram_kernel_size = object_property_get_int(first_mem_node,
                                                              "size", &errp);

        if (kernel_info.ram_kernel_size == -1) {
            kernel_info.ram_kernel_size = ram_size;
        }

        if (zynq_7000 && !memory_region_is_mapped(mr)) {
            memory_region_add_subregion(get_system_memory(),
                                        kernel_info.ram_kernel_base, mr);
        }
    } else {
        kernel_info.ram_kernel_base = 0;
        kernel_info.ram_kernel_size = hwdtb->fulfilled_ram_amount;
    }

    hwdtb_free(hwdtb);

    return kernel_info;
}

static void arm_generic_fdt_init(MachineState *machine)
{
    void *fdt = NULL, *sw_fdt = NULL;
    int fdt_size, sw_fdt_size;
    const char *dtb_arg, *hw_dtb_arg;
    char node_path[DT_PATH_LENGTH];
    char *qspi_clone_spi_flash_node_name = NULL;
    memory_info kernel_info;
    bool zynq_7000 = false;
    int is_linux;

    is_linux = object_property_get_bool(OBJECT(qdev_get_machine()),
                                        "linux", NULL);

    /* If booting a Zynq-7000 Machine*/
    if (!strcmp(MACHINE_GET_CLASS(machine)->name, ZYNQ7000_MACHINE_NAME)) {
        zynq_7000 = true;
    } else if (!strcmp(MACHINE_GET_CLASS(machine)->name,
                       DEP_GENERAL_MACHINE_NAME)) {
        if (!QTEST_RUNNING) {
            /* Don't print this error if running qtest */
            fprintf(stderr, "The '" DEP_GENERAL_MACHINE_NAME "' machine has " \
                    "been deprecated and will be removed after the 2017.4 " \
                    "release.Please use '" ZYNQ7000_MACHINE_NAME \
                    "' instead.\n");
        }
        zynq_7000 = true;
    }

    dtb_arg = machine->dtb;
    hw_dtb_arg = machine->hw_dtb;
    if (!dtb_arg && !hw_dtb_arg) {
        if (!QTEST_RUNNING) {
            /* Just return without error if running qtest, as we never have a
             * device tree
             */
            hw_error("DTB must be specified for %s machine model\n",
                     MACHINE_GET_CLASS(machine)->name);
        }
        return;
    }

    /* Software dtb is always the -dtb arg */
    if (dtb_arg) {
        sw_fdt = load_device_tree(dtb_arg, &sw_fdt_size);
        if (!sw_fdt) {
            error_report("Error: Unable to load Device Tree %s", dtb_arg);
            exit(1);
        }
    }

    /* If the user provided a -hw-dtb, use it as the hw description.  */
    if (!hw_dtb_arg) {
        hw_dtb_arg = dtb_arg;
    }

    fdt = load_device_tree(hw_dtb_arg, &fdt_size);
    if (!fdt) {
        error_report("Error: Unable to load Device Tree %s", hw_dtb_arg);
        exit(1);
    }

    if (zynq_7000) {
        int node_offset = 0;

        /* Added a dummy flash node, if is-dual property is set to 1*/
        qspi_clone_spi_flash_node_name = zynq7000_qspi_flash_node_clone(fdt);

        /* Ensure that an interrupt controller exists before disabling it */
        if (!qemu_devtree_get_node_by_name(fdt, node_path,
                                           "interrupt-controller")) {
            qemu_fdt_setprop_cells(fdt, node_path,
                                   "disable-linux-gic-init", true);
        }

        /* The Zynq-7000 device tree doesn't contain information about the
         * Configuation Base Address Register (reset-cbar) but we need to set
         * it in order for Linux to find the SCU. So add it into the device
         * tree for every A9 CPU.
         */
        do {
            node_offset = fdt_node_offset_by_compatible(fdt, node_offset,
                                                        "arm,cortex-a9");
            if (node_offset > 0) {
                fdt_get_path(fdt, node_offset, node_path, DT_PATH_LENGTH);
                qemu_fdt_setprop_cells(fdt, node_path, "reset-cbar",
                                       ZYNQ7000_MPCORE_PERIPHBASE);
                qemu_fdt_setprop_cells(fdt, node_path, "midr",
                                       ZYNQ7000_MPCORE_REV);
            }
        } while (node_offset > 0);

        replace_compatible(fdt, "simple-bus", "qemu:system-memory");

        while ((node_offset = fdt_next_node(fdt, node_offset, NULL))) {
            if ((fdt_get_property(fdt, node_offset, "interrupt-names", NULL))) {
                fdt_delprop(fdt, node_offset, "interrupt-names");
                node_offset = 0;
            }
        }
    }

    kernel_info = init_machine(machine, fdt, machine->ram_size, zynq_7000);

    arm_generic_fdt_binfo.fdt = sw_fdt;
    arm_generic_fdt_binfo.fdt_size = sw_fdt_size;
    arm_generic_fdt_binfo.ram_size = kernel_info.ram_kernel_size;
    arm_generic_fdt_binfo.kernel_filename = machine->kernel_filename;
    arm_generic_fdt_binfo.kernel_cmdline = machine->kernel_cmdline;
    arm_generic_fdt_binfo.initrd_filename = machine->initrd_filename;
    arm_generic_fdt_binfo.write_secondary_boot = arm_write_secondary_boot;
    arm_generic_fdt_binfo.smp_loader_start = SMP_BOOT_ADDR;
    arm_generic_fdt_binfo.smp_bootreg_addr = SMP_BOOTREG_ADDR;
    arm_generic_fdt_binfo.board_id = 0xd32;
    arm_generic_fdt_binfo.loader_start = kernel_info.ram_kernel_base;
    arm_generic_fdt_binfo.secure_boot = is_linux && !zynq_7000 ? false : true;

    if (qspi_clone_spi_flash_node_name != NULL) {
        /* Remove cloned DTB node */
        int offset = fdt_path_offset(fdt, qspi_clone_spi_flash_node_name);
        fdt_del_node(fdt, offset);
        g_free(qspi_clone_spi_flash_node_name);
    }

    if (zynq_7000) {
        zynq7000_usb_nuke_phy(sw_fdt);
    }

    if (machine->kernel_filename) {
        arm_load_kernel(ARM_CPU(first_cpu), machine, &arm_generic_fdt_binfo);
    }

    return;
}

static void arm_generic_fdt_7000_init(MachineState *machine)
{
    MemoryRegion *address_space_mem = get_system_memory();
    DeviceState *dev;
    SysBusDevice *busdev;
    MemoryRegion *ocm_ram;
    DriveInfo *dinfo;
    DeviceState *att_dev;
    uint32_t num_cpus = 0;
    CPUState *cpu;

    ocm_ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ocm_ram, NULL, "zynq.ocm_ram", 256 << 10,
                           &error_abort);
    memory_region_add_subregion(address_space_mem, 0xFFFC0000, ocm_ram);

    dev = qdev_new("arm.pl35x");
    object_property_add_child(machine_get_container("unattached"),
                              "pl353", OBJECT(dev));
    qdev_prop_set_uint8(dev, "x", 3);
    dinfo = drive_get_next(IF_PFLASH);
    att_dev = nand_init(dinfo ? blk_by_legacy_dinfo(dinfo)
                              : NULL,
                        NAND_MFR_STMICRO, 0xaa);
    object_property_set_link(OBJECT(dev), "dev1", OBJECT(att_dev), &error_abort);

    busdev = SYS_BUS_DEVICE(dev);
    sysbus_realize(busdev, &error_fatal);
    sysbus_mmio_map(busdev, 0, 0xe000e000);
    sysbus_mmio_map(busdev, 2, 0xe1000000);

    arm_generic_fdt_init(machine);

    CPU_FOREACH(cpu) {
        num_cpus++;
    }

    dev = qdev_new("a9-scu");
    busdev = SYS_BUS_DEVICE(dev);
    qdev_prop_set_uint32(dev, "num-cpu", num_cpus);
    sysbus_realize(busdev, &error_fatal);
    sysbus_mmio_map(busdev, 0, ZYNQ7000_MPCORE_PERIPHBASE);
}

static bool get_ignore_memory_transaction_failures(Object *obj, Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(obj);

    return mc->ignore_memory_transaction_failures;
}

static void set_ignore_memory_transaction_failures(Object *obj, bool value,
                                                   Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(obj);

    mc->ignore_memory_transaction_failures = value;
}

static void arm_generic_fdt_machine_init(MachineClass *mc)
{
    mc->desc = "ARM device tree driven machine model";
    mc->init = arm_generic_fdt_init;
    /* X A53s and 2 R5s */
    mc->max_cpus = 64;
    mc->default_cpus = 64;

    /*
     * Historically, this machine set the ignore_memory_transaction_failures
     * flag unconditionally. This does not match real hardware behavior and the
     * ultimate goal here is to get rid of it. As this is an intrusive and
     * breaking change for a lot of workloads, this property is introduced to
     * help do the transition. For now it is still true by default. The next
     * step is to turn it off by default.
     */
    mc->ignore_memory_transaction_failures = true;
    object_class_property_add_bool(OBJECT_CLASS(mc), "ignore-mem-tx-failures",
                                   get_ignore_memory_transaction_failures,
                                   set_ignore_memory_transaction_failures);
    object_class_property_set_description(OBJECT_CLASS(mc),
                                          "ignore-mem-tx-failures",
                                          "Ignore memory transaction failures "
                                          "(default: on)");
}

static void arm_generic_fdt_7000_machine_init(MachineClass *mc)
{
    mc->desc = "ARM device tree driven machine model for the Zynq-7000";
    mc->init = arm_generic_fdt_7000_init;
    mc->ignore_memory_transaction_failures = true;
    mc->max_cpus = 2;
    mc->default_cpus = 2;
}

/* Deprecated, remove this */
static void arm_generic_fdt_dep_machine_init(MachineClass *mc)
{
    mc->desc = "Deprecated ARM device tree driven machine for the Zynq-7000";
    mc->init = arm_generic_fdt_7000_init;
    mc->ignore_memory_transaction_failures = true;
    mc->max_cpus = 2;
    mc->default_cpus = 2;
}

static void arm_generic_fdt_machine_plug(HotplugHandler *hotplug_dev,
                                         DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(hotplug_dev);

    if (!ms->dynamic_mem) {
        return;
    }

    if (object_dynamic_cast(OBJECT(dev), TYPE_AMD_DDR_MEMORY)) {
        AMDDDRMemory *ddr = AMD_DDR_MEMORY(dev);
        Object *mem_obj;
        MemoryRegion *c;

        /* DDR low is handled by the hwdtb */
        if (ddr->address < DDR_LOW_SIZE) {
            return;
        }

        mem_obj = object_resolve_path_type("/machine/hwdtb<memory@00000000>",
                                           TYPE_MEMORY_REGION, NULL);
        if (!mem_obj) {
            mem_obj = object_resolve_path_type("/machine/hwdtb<memory@0>",
                                               TYPE_MEMORY_REGION, NULL);
        }
        if (!mem_obj) {
            error_setg(errp,
                       "/memory@00000000 or /memory@0 is missing in the FDT");
            return;
        }

        c = MEMORY_REGION(mem_obj);

        memory_region_add_subregion(c, ddr->address, &ddr->mr);
    }
}

static HotplugHandler *arm_generic_fdt_get_hotplug_handler(MachineState *ms,
                                                           DeviceState *dev)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_AMD_DDR_MEMORY)) {
        return HOTPLUG_HANDLER(ms);
    }
    return NULL;
}

static void arm_generic_fdt_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(oc);

    arm_generic_fdt_machine_init(mc);

    mc->get_hotplug_handler = arm_generic_fdt_get_hotplug_handler;

    hc->plug = arm_generic_fdt_machine_plug;
}

static const TypeInfo arm_generic_fdt_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME(GENERAL_MACHINE_NAME),
    .parent     = TYPE_MACHINE,
    .class_init = arm_generic_fdt_machine_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_HOTPLUG_HANDLER },
        { TYPE_TARGET_ARM_MACHINE },
        { TYPE_TARGET_AARCH64_MACHINE },
        { }
    },
};

static void arm_generic_fdt_machine_register_types(void)
{
    type_register_static(&arm_generic_fdt_machine_typeinfo);
}
type_init(arm_generic_fdt_machine_register_types)

DEFINE_MACHINE_AARCH64(ZYNQ7000_MACHINE_NAME, arm_generic_fdt_7000_machine_init)
DEFINE_MACHINE_AARCH64(DEP_GENERAL_MACHINE_NAME, arm_generic_fdt_dep_machine_init)
