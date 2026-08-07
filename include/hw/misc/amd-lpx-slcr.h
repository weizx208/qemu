/*
 * QEMU model of the LPX_SLCR Global system level control registers
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AMD_LPX_SLCR_H
#define AMD_LPX_SLCR_H

#include "hw/sysbus.h"
#include "hw/register.h"
#include "qom/object.h"

#define TYPE_AMD_LPX_SLCR "amd-lpx-slcr"

#define LPX_SLCR_R_MAX (0x54c / 4 + 1)

struct AmdLpxSlcr {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_rpu_clustera_err_imr;
    qemu_irq irq_rpu_clusterb_err_imr;
    qemu_irq irq_nmu_rd_firewall_imr;
    qemu_irq irq_nsu_firewall_imr;
    qemu_irq irq_zdma_ls_cmp_out_imr;
    qemu_irq irq_rpu_pcil_imr;
    qemu_irq irq_nmu_wr_firewall_imr;
    qemu_irq irq_nmu_wr_firewall_ien;
    qemu_irq irq_zdma_sync_ls_cmp_out_imr;

    uint32_t regs[LPX_SLCR_R_MAX];
    RegisterInfo regs_info[LPX_SLCR_R_MAX];
};

OBJECT_DECLARE_SIMPLE_TYPE(AmdLpxSlcr, AMD_LPX_SLCR)

#endif /* AMD_LPX_SLCR_H */
