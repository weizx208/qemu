/*
 * QEMU model of the FPX_SLCR Global system level control registers
 * for the full power domain
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/amd-fpx-slcr.h"
#include "hw/register.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/irq.h"

#ifndef AMD_FPX_SLCR_ERR_DEBUG
#define AMD_FPX_SLCR_ERR_DEBUG 0
#endif

REG32(WPROT0, 0x0)
    FIELD(WPROT0, ACTIVE, 0, 1)
REG32(SAFETY_CHK0, 0x60)
REG32(WWDT0_CLK_SEL, 0x100)
    FIELD(WWDT0_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT0_CLK_SEL, SELECT_0, 0, 1)
REG32(WWDT1_CLK_SEL, 0x104)
    FIELD(WWDT1_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT1_CLK_SEL, SELECT_0, 0, 1)
REG32(WWDT2_CLK_SEL, 0x108)
    FIELD(WWDT2_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT2_CLK_SEL, SELECT_0, 0, 1)
REG32(WWDT3_CLK_SEL, 0x10c)
    FIELD(WWDT3_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT3_CLK_SEL, SELECT_0, 0, 1)
REG32(BISR_CACHE_CTRL_0, 0x400)
    FIELD(BISR_CACHE_CTRL_0, CLR, 4, 1)
    FIELD(BISR_CACHE_CTRL_0, TRIGGER, 0, 1)
REG32(BISR_CACHE_CTRL_1, 0x404)
    FIELD(BISR_CACHE_CTRL_1, PGEN_20, 20, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_19, 19, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_18, 18, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_17, 17, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_16, 16, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_15, 15, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_14, 14, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_13, 13, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_12, 12, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_11, 11, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_10, 10, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_9, 9, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_8, 8, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_7, 7, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_6, 6, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_5, 5, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_4, 4, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_3, 3, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_2, 2, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_1, 1, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_0, 0, 1)
REG32(BISR_CACHE_STATUS0, 0x408)
    FIELD(BISR_CACHE_STATUS0, PASS_14, 31, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_14, 30, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_13, 29, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_13, 28, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_12, 27, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_12, 26, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_11, 25, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_11, 24, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_10, 23, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_10, 22, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_9, 21, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_9, 20, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_8, 19, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_8, 18, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_7, 17, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_7, 16, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_6, 15, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_6, 14, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_5, 13, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_5, 12, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_4, 11, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_4, 10, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_3, 9, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_3, 8, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_2, 7, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_2, 6, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_1, 5, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_1, 4, 1)
    FIELD(BISR_CACHE_STATUS0, PASS_0, 3, 1)
    FIELD(BISR_CACHE_STATUS0, DONE_0, 2, 1)
    FIELD(BISR_CACHE_STATUS0, PASS, 1, 1)
    FIELD(BISR_CACHE_STATUS0, DONE, 0, 1)
REG32(BISR_CACHE_STATUS1, 0x40c)
    FIELD(BISR_CACHE_STATUS1, PASS_20, 11, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_20, 10, 1)
    FIELD(BISR_CACHE_STATUS1, PASS_19, 9, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_19, 8, 1)
    FIELD(BISR_CACHE_STATUS1, PASS_18, 7, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_18, 6, 1)
    FIELD(BISR_CACHE_STATUS1, PASS_17, 5, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_17, 4, 1)
    FIELD(BISR_CACHE_STATUS1, PASS_16, 3, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_16, 2, 1)
    FIELD(BISR_CACHE_STATUS1, PASS_15, 1, 1)
    FIELD(BISR_CACHE_STATUS1, DONE_15, 0, 1)
REG32(APU_CTRL, 0x1000)
    FIELD(APU_CTRL, ACP_PORT_EN, 5, 4)
    FIELD(APU_CTRL, ACP_CHK, 4, 1)
    FIELD(APU_CTRL, APU_LOCKSTEP_MODE, 0, 4)
REG32(APU0_SB_MBIST_CTRL, 0x1010)
    FIELD(APU0_SB_MBIST_CTRL, DFTRAMHOLD, 2, 1)
    FIELD(APU0_SB_MBIST_CTRL, NRESET, 1, 1)
    FIELD(APU0_SB_MBIST_CTRL, REQ, 0, 1)
REG32(APU1_SB_MBIST_CTRL, 0x1014)
    FIELD(APU1_SB_MBIST_CTRL, DFTRAMHOLD, 2, 1)
    FIELD(APU1_SB_MBIST_CTRL, NRESET, 1, 1)
    FIELD(APU1_SB_MBIST_CTRL, REQ, 0, 1)
REG32(APU2_SB_MBIST_CTRL, 0x1018)
    FIELD(APU2_SB_MBIST_CTRL, DFTRAMHOLD, 2, 1)
    FIELD(APU2_SB_MBIST_CTRL, NRESET, 1, 1)
    FIELD(APU2_SB_MBIST_CTRL, REQ, 0, 1)
REG32(APU3_SB_MBIST_CTRL, 0x101c)
    FIELD(APU3_SB_MBIST_CTRL, DFTRAMHOLD, 2, 1)
    FIELD(APU3_SB_MBIST_CTRL, NRESET, 1, 1)
    FIELD(APU3_SB_MBIST_CTRL, REQ, 0, 1)
REG32(ACP_CTRL, 0x1100)
    FIELD(ACP_CTRL, ACP_SRC_SEL, 2, 1)
    FIELD(ACP_CTRL, ACP_DEST_ID, 0, 2)
REG32(PKI_MUX_SEL, 0x2000)
    FIELD(PKI_MUX_SEL, PKI_SMUX_PATHSEL, 0, 1)
REG32(AFI_FM_0, 0x4000)
    FIELD(AFI_FM_0, PL_INTF_MODE, 0, 1)
REG32(AFI_FM_1, 0x4004)
    FIELD(AFI_FM_1, PL_INTF_MODE, 0, 1)
REG32(AFI_FM_2, 0x4008)
    FIELD(AFI_FM_2, PL_INTF_MODE, 0, 1)
REG32(AFI_FM_3, 0x400c)
    FIELD(AFI_FM_3, PL_INTF_MODE, 0, 1)
REG32(PARITY_ERR_ISR, 0x8000)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_PKI, 21, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_CMN3, 20, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_CMN2, 19, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_CMN1, 18, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_CML, 17, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU11, 16, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU10, 15, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU9, 14, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU8, 13, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU7, 12, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU6, 11, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU5, 10, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU4, 9, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU3, 8, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU2, 7, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU1, 6, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_MMU0, 5, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_INTWRAP_AFIFM3, 4, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_INTWRAP_AFIFM2, 3, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_INTWRAP_AFIFM1, 2, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_INTWRAP_AFIFM0, 1, 1)
    FIELD(PARITY_ERR_ISR, PARITY_ERR_GIC, 0, 1)
REG32(PARITY_ERR_IMR, 0x8004)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_PKI, 21, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_CMN3, 20, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_CMN2, 19, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_CMN1, 18, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_CML, 17, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU11, 16, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU10, 15, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU9, 14, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU8, 13, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU7, 12, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU6, 11, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU5, 10, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU4, 9, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU3, 8, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU2, 7, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU1, 6, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_MMU0, 5, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_INTWRAP_AFIFM3, 4, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_INTWRAP_AFIFM2, 3, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_INTWRAP_AFIFM1, 2, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_INTWRAP_AFIFM0, 1, 1)
    FIELD(PARITY_ERR_IMR, PARITY_ERR_GIC, 0, 1)
REG32(PARITY_ERR_IEN, 0x8008)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_PKI, 21, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_CMN3, 20, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_CMN2, 19, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_CMN1, 18, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_CML, 17, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU11, 16, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU10, 15, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU9, 14, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU8, 13, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU7, 12, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU6, 11, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU5, 10, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU4, 9, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU3, 8, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU2, 7, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU1, 6, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_MMU0, 5, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_INTWRAP_AFIFM3, 4, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_INTWRAP_AFIFM2, 3, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_INTWRAP_AFIFM1, 2, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_INTWRAP_AFIFM0, 1, 1)
    FIELD(PARITY_ERR_IEN, PARITY_ERR_GIC, 0, 1)
REG32(PARITY_ERR_IDS, 0x800c)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_PKI, 21, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_CMN3, 20, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_CMN2, 19, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_CMN1, 18, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_CML, 17, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU11, 16, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU10, 15, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU9, 14, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU8, 13, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU7, 12, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU6, 11, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU5, 10, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU4, 9, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU3, 8, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU2, 7, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU1, 6, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_MMU0, 5, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_INTWRAP_AFIFM3, 4, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_INTWRAP_AFIFM2, 3, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_INTWRAP_AFIFM1, 2, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_INTWRAP_AFIFM0, 1, 1)
    FIELD(PARITY_ERR_IDS, PARITY_ERR_GIC, 0, 1)
REG32(FPX_DFX_ERR_ISR, 0x8014)
    FIELD(FPX_DFX_ERR_ISR, PKI_DFX_ERR, 7, 1)
    FIELD(FPX_DFX_ERR_ISR, FPXINT_DFX_ERR, 6, 1)
    FIELD(FPX_DFX_ERR_ISR, MMU_DFX_ERR, 5, 1)
    FIELD(FPX_DFX_ERR_ISR, CML_DFX_ERR, 4, 1)
    FIELD(FPX_DFX_ERR_ISR, CMN1_DFX_ERR, 3, 1)
    FIELD(FPX_DFX_ERR_ISR, CMN2_DFX_ERR, 2, 1)
    FIELD(FPX_DFX_ERR_ISR, CMN3_1_DFX_ERR, 1, 1)
    FIELD(FPX_DFX_ERR_ISR, CMN3_0_DFX_ERR, 0, 1)
REG32(FPX_DFX_ERR_IMR, 0x8018)
    FIELD(FPX_DFX_ERR_IMR, PKI_DFX_ERR, 7, 1)
    FIELD(FPX_DFX_ERR_IMR, FPXINT_DFX_ERR, 6, 1)
    FIELD(FPX_DFX_ERR_IMR, MMU_DFX_ERR, 5, 1)
    FIELD(FPX_DFX_ERR_IMR, CML_DFX_ERR, 4, 1)
    FIELD(FPX_DFX_ERR_IMR, CMN1_DFX_ERR, 3, 1)
    FIELD(FPX_DFX_ERR_IMR, CMN2_DFX_ERR, 2, 1)
    FIELD(FPX_DFX_ERR_IMR, CMN3_1_DFX_ERR, 1, 1)
    FIELD(FPX_DFX_ERR_IMR, CMN3_0_DFX_ERR, 0, 1)
REG32(FPX_DFX_ERR_IEN, 0x801c)
    FIELD(FPX_DFX_ERR_IEN, PKI_DFX_ERR, 7, 1)
    FIELD(FPX_DFX_ERR_IEN, FPXINT_DFX_ERR, 6, 1)
    FIELD(FPX_DFX_ERR_IEN, MMU_DFX_ERR, 5, 1)
    FIELD(FPX_DFX_ERR_IEN, CML_DFX_ERR, 4, 1)
    FIELD(FPX_DFX_ERR_IEN, CMN1_DFX_ERR, 3, 1)
    FIELD(FPX_DFX_ERR_IEN, CMN2_DFX_ERR, 2, 1)
    FIELD(FPX_DFX_ERR_IEN, CMN3_1_DFX_ERR, 1, 1)
    FIELD(FPX_DFX_ERR_IEN, CMN3_0_DFX_ERR, 0, 1)
REG32(FPX_DFX_ERR_IDS, 0x8020)
    FIELD(FPX_DFX_ERR_IDS, PKI_DFX_ERR, 7, 1)
    FIELD(FPX_DFX_ERR_IDS, FPXINT_DFX_ERR, 6, 1)
    FIELD(FPX_DFX_ERR_IDS, MMU_DFX_ERR, 5, 1)
    FIELD(FPX_DFX_ERR_IDS, CML_DFX_ERR, 4, 1)
    FIELD(FPX_DFX_ERR_IDS, CMN1_DFX_ERR, 3, 1)
    FIELD(FPX_DFX_ERR_IDS, CMN2_DFX_ERR, 2, 1)
    FIELD(FPX_DFX_ERR_IDS, CMN3_1_DFX_ERR, 1, 1)
    FIELD(FPX_DFX_ERR_IDS, CMN3_0_DFX_ERR, 0, 1)
REG32(FPX_DFX_ERR_TRIGGER, 0x8024)
    FIELD(FPX_DFX_ERR_TRIGGER, PKI_DFX_ERR, 7, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, FPXINT_DFX_ERR, 6, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, MMU_DFX_ERR, 5, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, CML_DFX_ERR, 4, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, CMN1_DFX_ERR, 3, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, CMN2_DFX_ERR, 2, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, CMN3_1_DFX_ERR, 1, 1)
    FIELD(FPX_DFX_ERR_TRIGGER, CMN3_0_DFX_ERR, 0, 1)

#define FPX_SLCR_MMIO_SIZE  0x10000

static void parity_err_imr_update_irq(AmdFpxSlcr *s)
{
    bool pending = s->parity_err_isr & ~s->parity_err_imr;
    qemu_set_irq(s->irq_parity_err_imr, pending);
}

static void fpx_dfx_err_imr_update_irq(AmdFpxSlcr *s)
{
    bool pending = s->fpx_dfx_err_isr & ~s->fpx_dfx_err_imr;
    qemu_set_irq(s->irq_fpx_dfx_err_imr, pending);
}

static void fpx_slcr_reset_enter(Object *obj, ResetType type)
{
    AmdFpxSlcr *s = AMD_FPX_SLCR(obj);

    memset(s->regs, 0, sizeof(s->regs));
    s->apu_ctrl = 0;
    s->acp_ctrl = 0;
    s->pki_mux_sel = 0;
    s->parity_err_isr = 0;
    s->fpx_dfx_err_isr = 0;

    s->regs[R_WPROT0] = 0x1;
    s->apu_sb_mbist_ctrl[0] = 0x2;
    s->apu_sb_mbist_ctrl[1] = 0x2;
    s->apu_sb_mbist_ctrl[2] = 0x2;
    s->apu_sb_mbist_ctrl[3] = 0x2;
    s->afi_fm[0] = 0x1;
    s->afi_fm[1] = 0x1;
    s->afi_fm[2] = 0x1;
    s->afi_fm[3] = 0x1;
    s->parity_err_imr = 0x3fffff;
    s->fpx_dfx_err_imr = 0xff;
}

static uint64_t fpx_slcr_read(void *opaque, hwaddr addr, unsigned size)
{
    AmdFpxSlcr *s = AMD_FPX_SLCR(opaque);
    uint32_t r = addr / 4;

    switch (addr) {
    case A_WPROT0:
    case A_SAFETY_CHK0:
    case A_WWDT0_CLK_SEL ... A_WWDT3_CLK_SEL:
    case A_BISR_CACHE_CTRL_0:
    case A_BISR_CACHE_CTRL_1:
    case A_BISR_CACHE_STATUS0:
    case A_BISR_CACHE_STATUS1:
        return s->regs[r];

    case A_APU_CTRL:
        return s->apu_ctrl;
    case A_APU0_SB_MBIST_CTRL ... A_APU3_SB_MBIST_CTRL:
        return s->apu_sb_mbist_ctrl[(addr - A_APU0_SB_MBIST_CTRL) / 4];
    case A_ACP_CTRL:
        return s->acp_ctrl;
    case A_PKI_MUX_SEL:
        return s->pki_mux_sel;
    case A_AFI_FM_0 ... A_AFI_FM_3:
        return s->afi_fm[(addr - A_AFI_FM_0) / 4];
    case A_PARITY_ERR_ISR:
        return s->parity_err_isr;
    case A_PARITY_ERR_IMR:
        return s->parity_err_imr;
    case A_FPX_DFX_ERR_ISR:
        return s->fpx_dfx_err_isr;
    case A_FPX_DFX_ERR_IMR:
        return s->fpx_dfx_err_imr;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "fpx_slcr: read from invalid offset 0x%"
                      HWADDR_PRIx "\n", addr);
        return 0;
    }
}

static void fpx_slcr_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    AmdFpxSlcr *s = AMD_FPX_SLCR(opaque);
    uint32_t val = (uint32_t)value;
    uint32_t r = addr / 4;

    switch (addr) {
    case A_PARITY_ERR_ISR:
        s->parity_err_isr &= ~val;
        parity_err_imr_update_irq(s);
        break;
    case A_FPX_DFX_ERR_ISR:
        s->fpx_dfx_err_isr &= ~val;
        fpx_dfx_err_imr_update_irq(s);
        break;
    case A_PARITY_ERR_IEN:
        s->parity_err_imr &= ~val;
        parity_err_imr_update_irq(s);
        return;
    case A_FPX_DFX_ERR_IEN:
        s->fpx_dfx_err_imr &= ~val;
        fpx_dfx_err_imr_update_irq(s);
        return;
    case A_PARITY_ERR_IDS:
        s->parity_err_imr |= val;
        parity_err_imr_update_irq(s);
        return;
    case A_FPX_DFX_ERR_IDS:
        s->fpx_dfx_err_imr |= val;
        fpx_dfx_err_imr_update_irq(s);
        return;
    case A_FPX_DFX_ERR_TRIGGER:
        s->fpx_dfx_err_isr |= val;
        fpx_dfx_err_imr_update_irq(s);
        return;
    case A_BISR_CACHE_CTRL_0:
        s->regs[r] = val & 0x11;
        return;
    case A_BISR_CACHE_CTRL_1:
        s->regs[r] = val & 0x1fffff;
        return;
    case A_WPROT0:
    case A_SAFETY_CHK0:
    case A_WWDT0_CLK_SEL ... A_WWDT3_CLK_SEL:
        s->regs[r] = val;
        return;

    case A_APU_CTRL:
        s->apu_ctrl = val;
        return;
    case A_APU0_SB_MBIST_CTRL ... A_APU3_SB_MBIST_CTRL:
        s->apu_sb_mbist_ctrl[(addr - A_APU0_SB_MBIST_CTRL) / 4] = val;
        return;
    case A_ACP_CTRL:
        s->acp_ctrl = val;
        return;
    case A_PKI_MUX_SEL:
        s->pki_mux_sel = val;
        return;
    case A_AFI_FM_0 ... A_AFI_FM_3:
        s->afi_fm[(addr - A_AFI_FM_0) / 4] = val;
        return;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "fpx_slcr: write to invalid offset 0x%"
                      HWADDR_PRIx "\n", addr);
        return;
    }
}

static void fpx_slcr_reset_hold(Object *obj, ResetType type)
{
    AmdFpxSlcr *s = AMD_FPX_SLCR(obj);

    parity_err_imr_update_irq(s);
    fpx_dfx_err_imr_update_irq(s);
}

static const MemoryRegionOps fpx_slcr_ops = {
    .read = fpx_slcr_read,
    .write = fpx_slcr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void fpx_slcr_init(Object *obj)
{
    AmdFpxSlcr *s = AMD_FPX_SLCR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &fpx_slcr_ops, s,
                          TYPE_AMD_FPX_SLCR, FPX_SLCR_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq_parity_err_imr);
    sysbus_init_irq(sbd, &s->irq_fpx_dfx_err_imr);
}

static const VMStateDescription vmstate_fpx_slcr = {
    .name = TYPE_AMD_FPX_SLCR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AmdFpxSlcr, FPX_SLCR_R_MAX),
        VMSTATE_UINT32(apu_ctrl, AmdFpxSlcr),
        VMSTATE_UINT32_ARRAY(apu_sb_mbist_ctrl, AmdFpxSlcr, 4),
        VMSTATE_UINT32(acp_ctrl, AmdFpxSlcr),
        VMSTATE_UINT32(pki_mux_sel, AmdFpxSlcr),
        VMSTATE_UINT32_ARRAY(afi_fm, AmdFpxSlcr, 4),
        VMSTATE_UINT32(parity_err_isr, AmdFpxSlcr),
        VMSTATE_UINT32(parity_err_imr, AmdFpxSlcr),
        VMSTATE_UINT32(fpx_dfx_err_isr, AmdFpxSlcr),
        VMSTATE_UINT32(fpx_dfx_err_imr, AmdFpxSlcr),
        VMSTATE_END_OF_LIST(),
    }
};

static void fpx_slcr_class_init(ObjectClass *klass, const void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_fpx_slcr;
    rc->phases.enter = fpx_slcr_reset_enter;
    rc->phases.hold = fpx_slcr_reset_hold;
}

static const TypeInfo fpx_slcr_info = {
    .name          = TYPE_AMD_FPX_SLCR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AmdFpxSlcr),
    .class_init    = fpx_slcr_class_init,
    .instance_init = fpx_slcr_init,
};

static void fpx_slcr_register_types(void)
{
    type_register_static(&fpx_slcr_info);
}

type_init(fpx_slcr_register_types)
