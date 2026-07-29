/*
 * QEMU model of the FPX_SLCR Global system level control registers
 * for the full power domain
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AMD_FPX_SLCR_H
#define AMD_FPX_SLCR_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_AMD_FPX_SLCR "amd-fpx-slcr"

#define FPX_SLCR_R_MAX (0x6ac / 4 + 1)

struct AmdFpxSlcr {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_parity_err_imr;
    qemu_irq irq_fpx_dfx_err_imr;

    uint32_t regs[FPX_SLCR_R_MAX];

    /* Sparse upper registers */
    uint32_t apu_ctrl;
    uint32_t apu_sb_mbist_ctrl[4];
    uint32_t acp_ctrl;
    uint32_t pki_mux_sel;
    uint32_t afi_fm[4];
    uint32_t parity_err_isr;
    uint32_t parity_err_imr;
    uint32_t fpx_dfx_err_isr;
    uint32_t fpx_dfx_err_imr;
    /* Private upper registers */
    uint32_t eco;
    uint32_t fpx_read_ncc;
    uint32_t fpx_write_ncc;
    uint32_t cpm_pcie_stash;
    uint32_t pl_loopback;
};

OBJECT_DECLARE_SIMPLE_TYPE(AmdFpxSlcr, AMD_FPX_SLCR)

#endif /* AMD_FPX_SLCR_H */
