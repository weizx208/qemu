/*
 * QEMU model of the LPX_SLCR Global system level control registers
 * for the low power domain
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/amd-lpx-slcr.h"
#include "hw/register.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "migration/vmstate.h"

#ifndef AMD_LPX_SLCR_ERR_DEBUG
#define AMD_LPX_SLCR_ERR_DEBUG 0
#endif

REG32(WPROT0, 0x0)
    FIELD(WPROT0, ACTIVE, 0, 1)
REG32(SAFETY_CHK0, 0x60)
REG32(SAFETY_CHK1, 0x64)
REG32(SAFETY_CHK2, 0x68)
REG32(SAFETY_CHK3, 0x6c)
REG32(SMID_CONFIG_ADMA0, 0x74)
    FIELD(SMID_CONFIG_ADMA0, CH7, 7, 1)
    FIELD(SMID_CONFIG_ADMA0, CH6, 6, 1)
    FIELD(SMID_CONFIG_ADMA0, CH5, 5, 1)
    FIELD(SMID_CONFIG_ADMA0, CH4, 4, 1)
    FIELD(SMID_CONFIG_ADMA0, CH3, 3, 1)
    FIELD(SMID_CONFIG_ADMA0, CH2, 2, 1)
    FIELD(SMID_CONFIG_ADMA0, CH1, 1, 1)
    FIELD(SMID_CONFIG_ADMA0, CH0, 0, 1)
REG32(HSDP_CONFIG, 0x88)
    FIELD(HSDP_CONFIG, LINK_REACH, 3, 1)
    FIELD(HSDP_CONFIG, AURORA_XPIPE_SEL, 2, 1)
    FIELD(HSDP_CONFIG, SEL_AUR_PCIE, 1, 1)
    FIELD(HSDP_CONFIG, SEL_AUR_PL, 0, 1)
REG32(OCM2_CONFIG, 0x8c)
    FIELD(OCM2_CONFIG, PRESENT, 0, 1)
REG32(BISR_CACHE_CTRL_0, 0x100)
    FIELD(BISR_CACHE_CTRL_0, CLR, 4, 1)
    FIELD(BISR_CACHE_CTRL_0, TRIGGER, 0, 1)
REG32(BISR_CACHE_CTRL_1, 0x104)
    FIELD(BISR_CACHE_CTRL_1, PGEN_4, 4, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_3, 3, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_2, 2, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_1, 1, 1)
    FIELD(BISR_CACHE_CTRL_1, PGEN_0, 0, 1)
REG32(BISR_CACHE_STATUS, 0x108)
    FIELD(BISR_CACHE_STATUS, PASS_GLOBAL, 31, 1)
    FIELD(BISR_CACHE_STATUS, DONE_GLOBAL, 30, 1)
    FIELD(BISR_CACHE_STATUS, PASS_4, 9, 1)
    FIELD(BISR_CACHE_STATUS, DONE_4, 8, 1)
    FIELD(BISR_CACHE_STATUS, PASS_3, 7, 1)
    FIELD(BISR_CACHE_STATUS, DONE_3, 6, 1)
    FIELD(BISR_CACHE_STATUS, PASS_2, 5, 1)
    FIELD(BISR_CACHE_STATUS, DONE_2, 4, 1)
    FIELD(BISR_CACHE_STATUS, PASS_1, 3, 1)
    FIELD(BISR_CACHE_STATUS, DONE_1, 2, 1)
    FIELD(BISR_CACHE_STATUS, PASS_0, 1, 1)
    FIELD(BISR_CACHE_STATUS, DONE_0, 0, 1)
REG32(BISR_CACHE_DATA_0, 0x10c)
REG32(BISR_CACHE_DATA_1, 0x110)
REG32(BISR_CACHE_DATA_2, 0x114)
REG32(BISR_CACHE_DATA_3, 0x118)
REG32(BISR_CACHE_DATA_4, 0x11c)
REG32(BISR_CACHE_DATA_5, 0x120)
REG32(BISR_CACHE_DATA_6, 0x124)
REG32(BISR_CACHE_DATA_7, 0x128)
REG32(NOC_TRANS_REMAP_RPU0A, 0x1f4)
    FIELD(NOC_TRANS_REMAP_RPU0A, NOC_TRANSACTION_REMAP, 0, 1)
REG32(NOC_TRANS_REMAP_RPU1A, 0x1f8)
    FIELD(NOC_TRANS_REMAP_RPU1A, NOC_TRANSACTION_REMAP, 0, 1)
REG32(NOC_TRANS_REMAP_RPU0B, 0x1fc)
    FIELD(NOC_TRANS_REMAP_RPU0B, NOC_TRANSACTION_REMAP, 0, 1)
REG32(NOC_TRANS_REMAP_RPU1B, 0x200)
    FIELD(NOC_TRANS_REMAP_RPU1B, NOC_TRANSACTION_REMAP, 0, 1)
REG32(SMID_CONFIG_RPU0A, 0x204)
    FIELD(SMID_CONFIG_RPU0A, CH3, 6, 2)
    FIELD(SMID_CONFIG_RPU0A, CH2, 4, 2)
    FIELD(SMID_CONFIG_RPU0A, CH1, 2, 2)
    FIELD(SMID_CONFIG_RPU0A, CH0, 0, 2)
REG32(SMID_CONFIG_RPU1A, 0x208)
    FIELD(SMID_CONFIG_RPU1A, CH3, 6, 2)
    FIELD(SMID_CONFIG_RPU1A, CH2, 4, 2)
    FIELD(SMID_CONFIG_RPU1A, CH1, 2, 2)
    FIELD(SMID_CONFIG_RPU1A, CH0, 0, 2)
REG32(SMID_CONFIG_RPU0B, 0x20c)
    FIELD(SMID_CONFIG_RPU0B, CH3, 6, 2)
    FIELD(SMID_CONFIG_RPU0B, CH2, 4, 2)
    FIELD(SMID_CONFIG_RPU0B, CH1, 2, 2)
    FIELD(SMID_CONFIG_RPU0B, CH0, 0, 2)
REG32(SMID_CONFIG_RPU1B, 0x210)
    FIELD(SMID_CONFIG_RPU1B, CH3, 6, 2)
    FIELD(SMID_CONFIG_RPU1B, CH2, 4, 2)
    FIELD(SMID_CONFIG_RPU1B, CH1, 2, 2)
    FIELD(SMID_CONFIG_RPU1B, CH0, 0, 2)
REG32(WWDT0_CLK_SEL, 0x220)
    FIELD(WWDT0_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT0_CLK_SEL, SELECT_0, 0, 1)
REG32(WWDT1_CLK_SEL, 0x224)
    FIELD(WWDT1_CLK_SEL, SELECT_1, 1, 1)
    FIELD(WWDT1_CLK_SEL, SELECT_0, 0, 1)
REG32(SSC_SWITCH_EN, 0x230)
    FIELD(SSC_SWITCH_EN, SELECT_EN, 0, 1)
REG32(ADMA0_ROUTE_CR_0, 0x280)
    FIELD(ADMA0_ROUTE_CR_0, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_1, 0x284)
    FIELD(ADMA0_ROUTE_CR_1, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_2, 0x288)
    FIELD(ADMA0_ROUTE_CR_2, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_3, 0x28c)
    FIELD(ADMA0_ROUTE_CR_3, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_4, 0x290)
    FIELD(ADMA0_ROUTE_CR_4, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_5, 0x294)
    FIELD(ADMA0_ROUTE_CR_5, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_6, 0x298)
    FIELD(ADMA0_ROUTE_CR_6, ROUTING_BIT, 0, 1)
REG32(ADMA0_ROUTE_CR_7, 0x29c)
    FIELD(ADMA0_ROUTE_CR_7, ROUTING_BIT, 0, 1)
REG32(NMU_RD_FIREWALL_ISR, 0x400)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_RD_FIREWALL_ISR, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_RD_FIREWALL_IMR, 0x404)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_RD_FIREWALL_IMR, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_RD_FIREWALL_IEN, 0x408)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_RD_FIREWALL_IEN, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_RD_FIREWALL_IDS, 0x40c)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_RD_FIREWALL_IDS, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_WR_FIREWALL_ISR, 0x410)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_WR_FIREWALL_ISR, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_WR_FIREWALL_IMR, 0x414)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_WR_FIREWALL_IMR, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_WR_FIREWALL_IEN, 0x418)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_WR_FIREWALL_IEN, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NMU_WR_FIREWALL_IDS, 0x41c)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_ISOC_AXI_ERR, 14, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_PCI_AXI3_ERR, 13, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_PCI_AXI2_ERR, 12, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_PCI_AXI1_ERR, 11, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_PCI_AXI0_ERR, 10, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PMX_NOC_AXI_ERR, 9, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_RPU_AXI_ERR, 8, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI7_ERR, 7, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI6_ERR, 6, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI5_ERR, 5, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI4_ERR, 4, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI3_ERR, 3, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI2_ERR, 2, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI1_ERR, 1, 1)
    FIELD(NMU_WR_FIREWALL_IDS, PSXL_NOC_CCI_AXI0_ERR, 0, 1)
REG32(NSU_FIREWALL_ISR, 0x420)
    FIELD(NSU_FIREWALL_ISR, NOC_PSXL_PCI_AXI_ERR, 5, 1)
    FIELD(NSU_FIREWALL_ISR, NOC_PMX_AXI_ERR, 4, 1)
    FIELD(NSU_FIREWALL_ISR, NOC_PSXL_CCI_AXI3_ERR, 3, 1)
    FIELD(NSU_FIREWALL_ISR, NOC_PSXL_CCI_AXI2_ERR, 2, 1)
    FIELD(NSU_FIREWALL_ISR, NOC_PSXL_CCI_AXI1_ERR, 1, 1)
    FIELD(NSU_FIREWALL_ISR, NOC_PSXL_CCI_AXI0_ERR, 0, 1)
REG32(NSU_FIREWALL_IMR, 0x424)
    FIELD(NSU_FIREWALL_IMR, NOC_PSXL_PCI_AXI_ERR, 5, 1)
    FIELD(NSU_FIREWALL_IMR, NOC_PMX_AXI_ERR, 4, 1)
    FIELD(NSU_FIREWALL_IMR, NOC_PSXL_CCI_AXI3_ERR, 3, 1)
    FIELD(NSU_FIREWALL_IMR, NOC_PSXL_CCI_AXI2_ERR, 2, 1)
    FIELD(NSU_FIREWALL_IMR, NOC_PSXL_CCI_AXI1_ERR, 1, 1)
    FIELD(NSU_FIREWALL_IMR, NOC_PSXL_CCI_AXI0_ERR, 0, 1)
REG32(NSU_FIREWALL_IEN, 0x428)
    FIELD(NSU_FIREWALL_IEN, NOC_PSXL_PCI_AXI_ERR, 5, 1)
    FIELD(NSU_FIREWALL_IEN, NOC_PMX_AXI_ERR, 4, 1)
    FIELD(NSU_FIREWALL_IEN, NOC_PSXL_CCI_AXI3_ERR, 3, 1)
    FIELD(NSU_FIREWALL_IEN, NOC_PSXL_CCI_AXI2_ERR, 2, 1)
    FIELD(NSU_FIREWALL_IEN, NOC_PSXL_CCI_AXI1_ERR, 1, 1)
    FIELD(NSU_FIREWALL_IEN, NOC_PSXL_CCI_AXI0_ERR, 0, 1)
REG32(NSU_FIREWALL_IDS, 0x42c)
    FIELD(NSU_FIREWALL_IDS, NOC_PSXL_PCI_AXI_ERR, 5, 1)
    FIELD(NSU_FIREWALL_IDS, NOC_PMX_AXI_ERR, 4, 1)
    FIELD(NSU_FIREWALL_IDS, NOC_PSXL_CCI_AXI3_ERR, 3, 1)
    FIELD(NSU_FIREWALL_IDS, NOC_PSXL_CCI_AXI2_ERR, 2, 1)
    FIELD(NSU_FIREWALL_IDS, NOC_PSXL_CCI_AXI1_ERR, 1, 1)
    FIELD(NSU_FIREWALL_IDS, NOC_PSXL_CCI_AXI0_ERR, 0, 1)
REG32(ZDMA_LS_CMP_OUT_ISR, 0x500)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_REG, 21, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_REG, 20, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_AFIFO_ZDMA, 19, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_AFIFO_ZDMA, 18, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI7, 15, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI6, 14, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI5, 13, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI4, 12, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI3, 11, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI2, 10, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI1, 9, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, OUT_FCI0, 8, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI7, 7, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI6, 6, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI5, 5, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI4, 4, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI3, 3, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI2, 2, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI1, 1, 1)
    FIELD(ZDMA_LS_CMP_OUT_ISR, ERR_FCI0, 0, 1)
REG32(ZDMA_LS_CMP_OUT_IMR, 0x504)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_REG, 21, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_REG, 20, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_AFIFO_ZDMA, 19, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_AFIFO_ZDMA, 18, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI7, 15, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI6, 14, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI5, 13, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI4, 12, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI3, 11, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI2, 10, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI1, 9, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, OUT_FCI0, 8, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI7, 7, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI6, 6, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI5, 5, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI4, 4, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI3, 3, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI2, 2, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI1, 1, 1)
    FIELD(ZDMA_LS_CMP_OUT_IMR, ERR_FCI0, 0, 1)
REG32(ZDMA_LS_CMP_OUT_IEN, 0x508)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_REG, 21, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_REG, 20, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_AFIFO_ZDMA, 19, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_AFIFO_ZDMA, 18, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI7, 15, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI6, 14, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI5, 13, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI4, 12, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI3, 11, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI2, 10, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI1, 9, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, OUT_FCI0, 8, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI7, 7, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI6, 6, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI5, 5, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI4, 4, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI3, 3, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI2, 2, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI1, 1, 1)
    FIELD(ZDMA_LS_CMP_OUT_IEN, ERR_FCI0, 0, 1)
REG32(ZDMA_LS_CMP_OUT_IDS, 0x50c)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_REG, 21, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_REG, 20, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_AFIFO_ZDMA, 19, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_AFIFO_ZDMA, 18, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI7, 15, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI6, 14, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI5, 13, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI4, 12, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI3, 11, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI2, 10, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI1, 9, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, OUT_FCI0, 8, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI7, 7, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI6, 6, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI5, 5, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI4, 4, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI3, 3, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI2, 2, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI1, 1, 1)
    FIELD(ZDMA_LS_CMP_OUT_IDS, ERR_FCI0, 0, 1)
REG32(RPU_PCIL_ISR, 0x510)
    FIELD(RPU_PCIL_ISR, RPU_PCIL_ERR_CORE_1B, 3, 1)
    FIELD(RPU_PCIL_ISR, RPU_PCIL_ERR_CORE_1A, 2, 1)
    FIELD(RPU_PCIL_ISR, RPU_PCIL_ERR_CORE_0B, 1, 1)
    FIELD(RPU_PCIL_ISR, RPU_PCIL_ERR_CORE_0A, 0, 1)
REG32(RPU_PCIL_IMR, 0x514)
    FIELD(RPU_PCIL_IMR, RPU_PCIL_ERR_CORE_1B, 3, 1)
    FIELD(RPU_PCIL_IMR, RPU_PCIL_ERR_CORE_1A, 2, 1)
    FIELD(RPU_PCIL_IMR, RPU_PCIL_ERR_CORE_0B, 1, 1)
    FIELD(RPU_PCIL_IMR, RPU_PCIL_ERR_CORE_0A, 0, 1)
REG32(RPU_PCIL_IEN, 0x518)
    FIELD(RPU_PCIL_IEN, RPU_PCIL_ERR_CORE_1B, 3, 1)
    FIELD(RPU_PCIL_IEN, RPU_PCIL_ERR_CORE_1A, 2, 1)
    FIELD(RPU_PCIL_IEN, RPU_PCIL_ERR_CORE_0B, 1, 1)
    FIELD(RPU_PCIL_IEN, RPU_PCIL_ERR_CORE_0A, 0, 1)
REG32(RPU_PCIL_IDS, 0x51c)
    FIELD(RPU_PCIL_IDS, RPU_PCIL_ERR_CORE_1B, 3, 1)
    FIELD(RPU_PCIL_IDS, RPU_PCIL_ERR_CORE_1A, 2, 1)
    FIELD(RPU_PCIL_IDS, RPU_PCIL_ERR_CORE_0B, 1, 1)
    FIELD(RPU_PCIL_IDS, RPU_PCIL_ERR_CORE_0A, 0, 1)
REG32(ZDMA_SYNC_LS_CMP_OUT_ISR, 0x520)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI7, 15, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI6, 14, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI5, 13, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI4, 12, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI3, 11, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI2, 10, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI1, 9, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, OUT_FCI0, 8, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI7, 7, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI6, 6, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI5, 5, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI4, 4, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI3, 3, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI2, 2, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI1, 1, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_ISR, ERR_FCI0, 0, 1)
REG32(ZDMA_SYNC_LS_CMP_OUT_IMR, 0x524)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI7, 15, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI6, 14, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI5, 13, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI4, 12, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI3, 11, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI2, 10, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI1, 9, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, OUT_FCI0, 8, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI7, 7, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI6, 6, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI5, 5, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI4, 4, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI3, 3, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI2, 2, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI1, 1, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IMR, ERR_FCI0, 0, 1)
REG32(ZDMA_SYNC_LS_CMP_OUT_IEN, 0x528)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI7, 15, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI6, 14, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI5, 13, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI4, 12, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI3, 11, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI2, 10, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI1, 9, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, OUT_FCI0, 8, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI7, 7, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI6, 6, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI5, 5, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI4, 4, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI3, 3, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI2, 2, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI1, 1, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IEN, ERR_FCI0, 0, 1)
REG32(ZDMA_SYNC_LS_CMP_OUT_IDS, 0x52c)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_ZDMA, 17, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_ZDMA, 16, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI7, 15, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI6, 14, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI5, 13, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI4, 12, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI3, 11, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI2, 10, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI1, 9, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, OUT_FCI0, 8, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI7, 7, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI6, 6, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI5, 5, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI4, 4, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI3, 3, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI2, 2, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI1, 1, 1)
    FIELD(ZDMA_SYNC_LS_CMP_OUT_IDS, ERR_FCI0, 0, 1)
REG32(RPU_CLUSTERA_ERR_ISR, 0x530)
    FIELD(RPU_CLUSTERA_ERR_ISR, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERA_ERR_ISR, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERA_ERR_ISR, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERA_ERR_ISR, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERA_ERR_IMR, 0x534)
    FIELD(RPU_CLUSTERA_ERR_IMR, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERA_ERR_IMR, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERA_ERR_IMR, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERA_ERR_IMR, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERA_ERR_IEN, 0x538)
    FIELD(RPU_CLUSTERA_ERR_IEN, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERA_ERR_IEN, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERA_ERR_IEN, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERA_ERR_IEN, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERA_ERR_IDS, 0x53c)
    FIELD(RPU_CLUSTERA_ERR_IDS, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERA_ERR_IDS, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERA_ERR_IDS, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERA_ERR_IDS, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERB_ERR_ISR, 0x540)
    FIELD(RPU_CLUSTERB_ERR_ISR, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERB_ERR_ISR, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERB_ERR_ISR, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERB_ERR_ISR, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERB_ERR_IMR, 0x544)
    FIELD(RPU_CLUSTERB_ERR_IMR, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERB_ERR_IMR, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERB_ERR_IMR, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERB_ERR_IMR, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERB_ERR_IEN, 0x548)
    FIELD(RPU_CLUSTERB_ERR_IEN, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERB_ERR_IEN, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERB_ERR_IEN, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERB_ERR_IEN, CLUSTER_ERR, 0, 1)
REG32(RPU_CLUSTERB_ERR_IDS, 0x54c)
    FIELD(RPU_CLUSTERB_ERR_IDS, CCF_ERR, 3, 1)
    FIELD(RPU_CLUSTERB_ERR_IDS, CORE1_ERR, 2, 1)
    FIELD(RPU_CLUSTERB_ERR_IDS, CORE0_ERR, 1, 1)
    FIELD(RPU_CLUSTERB_ERR_IDS, CLUSTER_ERR, 0, 1)

static void rpu_clustera_err_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_RPU_CLUSTERA_ERR_ISR] &
            ~s->regs[R_RPU_CLUSTERA_ERR_IMR];
    qemu_set_irq(s->irq_rpu_clustera_err_imr, pending);
}

static void rpu_clustera_err_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    rpu_clustera_err_imr_update_irq(s);
}

static uint64_t rpu_clustera_err_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_CLUSTERA_ERR_IMR] &= ~val;
    rpu_clustera_err_imr_update_irq(s);
    return 0;
}

static uint64_t rpu_clustera_err_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_CLUSTERA_ERR_IMR] |= val;
    rpu_clustera_err_imr_update_irq(s);
    return 0;
}

static void rpu_clusterb_err_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_RPU_CLUSTERB_ERR_ISR] &
            ~s->regs[R_RPU_CLUSTERB_ERR_IMR];
    qemu_set_irq(s->irq_rpu_clusterb_err_imr, pending);
}

static void rpu_clusterb_err_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    rpu_clusterb_err_imr_update_irq(s);
}

static uint64_t rpu_clusterb_err_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_CLUSTERB_ERR_IMR] &= ~val;
    rpu_clusterb_err_imr_update_irq(s);
    return 0;
}

static uint64_t rpu_clusterb_err_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_CLUSTERB_ERR_IMR] |= val;
    rpu_clusterb_err_imr_update_irq(s);
    return 0;
}

static void nmu_rd_firewall_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_NMU_RD_FIREWALL_ISR] &
            ~s->regs[R_NMU_RD_FIREWALL_IMR];
    qemu_set_irq(s->irq_nmu_rd_firewall_imr, pending);
}

static void nmu_rd_firewall_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    nmu_rd_firewall_imr_update_irq(s);
}

static uint64_t nmu_rd_firewall_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NMU_RD_FIREWALL_IMR] &= ~val;
    nmu_rd_firewall_imr_update_irq(s);
    return 0;
}

static uint64_t nmu_rd_firewall_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NMU_RD_FIREWALL_IMR] |= val;
    nmu_rd_firewall_imr_update_irq(s);
    return 0;
}

static void nsu_firewall_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_NSU_FIREWALL_ISR] &
            ~s->regs[R_NSU_FIREWALL_IMR];
    qemu_set_irq(s->irq_nsu_firewall_imr, pending);
}

static void nsu_firewall_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    nsu_firewall_imr_update_irq(s);
}

static uint64_t nsu_firewall_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NSU_FIREWALL_IMR] &= ~val;
    nsu_firewall_imr_update_irq(s);
    return 0;
}

static uint64_t nsu_firewall_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NSU_FIREWALL_IMR] |= val;
    nsu_firewall_imr_update_irq(s);
    return 0;
}

static void zdma_ls_cmp_out_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_ZDMA_LS_CMP_OUT_ISR] &
            ~s->regs[R_ZDMA_LS_CMP_OUT_IMR];
    qemu_set_irq(s->irq_zdma_ls_cmp_out_imr, pending);
}

static void zdma_ls_cmp_out_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    zdma_ls_cmp_out_imr_update_irq(s);
}

static uint64_t zdma_ls_cmp_out_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_ZDMA_LS_CMP_OUT_IMR] &= ~val;
    zdma_ls_cmp_out_imr_update_irq(s);
    return 0;
}

static uint64_t zdma_ls_cmp_out_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_ZDMA_LS_CMP_OUT_IMR] |= val;
    zdma_ls_cmp_out_imr_update_irq(s);
    return 0;
}

static void rpu_pcil_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_RPU_PCIL_ISR] &
            ~s->regs[R_RPU_PCIL_IMR];
    qemu_set_irq(s->irq_rpu_pcil_imr, pending);
}

static void rpu_pcil_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    rpu_pcil_imr_update_irq(s);
}

static uint64_t rpu_pcil_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_PCIL_IMR] &= ~val;
    rpu_pcil_imr_update_irq(s);
    return 0;
}

static uint64_t rpu_pcil_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_RPU_PCIL_IMR] |= val;
    rpu_pcil_imr_update_irq(s);
    return 0;
}

static void nmu_wr_firewall_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_NMU_WR_FIREWALL_ISR] &
            ~s->regs[R_NMU_WR_FIREWALL_IMR];
    qemu_set_irq(s->irq_nmu_wr_firewall_imr, pending);
}

static void nmu_wr_firewall_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    nmu_wr_firewall_imr_update_irq(s);
}

static uint64_t nmu_wr_firewall_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NMU_WR_FIREWALL_IMR] &= ~val;
    nmu_wr_firewall_imr_update_irq(s);
    return 0;
}

static uint64_t nmu_wr_firewall_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_NMU_WR_FIREWALL_IMR] |= val;
    nmu_wr_firewall_imr_update_irq(s);
    return 0;
}

static void zdma_sync_ls_cmp_out_imr_update_irq(AmdLpxSlcr *s)
{
    bool pending = s->regs[R_ZDMA_SYNC_LS_CMP_OUT_ISR] &
            ~s->regs[R_ZDMA_SYNC_LS_CMP_OUT_IMR];
    qemu_set_irq(s->irq_zdma_sync_ls_cmp_out_imr, pending);
}

static void zdma_sync_ls_cmp_out_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    zdma_sync_ls_cmp_out_imr_update_irq(s);
}

static uint64_t zdma_sync_ls_cmp_out_ien_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_ZDMA_SYNC_LS_CMP_OUT_IMR] &= ~val;
    zdma_sync_ls_cmp_out_imr_update_irq(s);
    return 0;
}

static uint64_t zdma_sync_ls_cmp_out_ids_prew(RegisterInfo *reg, uint64_t val64)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(reg->opaque);
    uint32_t val = val64;

    s->regs[R_ZDMA_SYNC_LS_CMP_OUT_IMR] |= val;
    zdma_sync_ls_cmp_out_imr_update_irq(s);
    return 0;
}

static const RegisterAccessInfo lpx_slcr_regs_info[] = {
    {   .name = "WPROT0",  .addr = A_WPROT0,
    },{ .name = "SAFETY_CHK0",  .addr = A_SAFETY_CHK0,
    },{ .name = "SAFETY_CHK1",  .addr = A_SAFETY_CHK1,
    },{ .name = "SAFETY_CHK2",  .addr = A_SAFETY_CHK2,
    },{ .name = "SAFETY_CHK3",  .addr = A_SAFETY_CHK3,
    },{ .name = "SMID_CONFIG_ADMA0",  .addr = A_SMID_CONFIG_ADMA0,
    },{ .name = "HSDP_CONFIG",  .addr = A_HSDP_CONFIG,
        .rsvd = 0xfffffff0,
    },{ .name = "OCM2_CONFIG",  .addr = A_OCM2_CONFIG,
        .rsvd = 0xfffffffe,
        .ro = 0x1,
    },{ .name = "BISR_CACHE_CTRL_0",  .addr = A_BISR_CACHE_CTRL_0,
        .rsvd = 0xe,
    },{ .name = "BISR_CACHE_CTRL_1",  .addr = A_BISR_CACHE_CTRL_1,
    },{ .name = "BISR_CACHE_STATUS",  .addr = A_BISR_CACHE_STATUS,
        .ro = 0xc00003ff,
    },{ .name = "BISR_CACHE_DATA_0",  .addr = A_BISR_CACHE_DATA_0,
    },{ .name = "BISR_CACHE_DATA_1",  .addr = A_BISR_CACHE_DATA_1,
    },{ .name = "BISR_CACHE_DATA_2",  .addr = A_BISR_CACHE_DATA_2,
    },{ .name = "BISR_CACHE_DATA_3",  .addr = A_BISR_CACHE_DATA_3,
    },{ .name = "BISR_CACHE_DATA_4",  .addr = A_BISR_CACHE_DATA_4,
    },{ .name = "BISR_CACHE_DATA_5",  .addr = A_BISR_CACHE_DATA_5,
    },{ .name = "BISR_CACHE_DATA_6",  .addr = A_BISR_CACHE_DATA_6,
    },{ .name = "BISR_CACHE_DATA_7",  .addr = A_BISR_CACHE_DATA_7,
    },{ .name = "NOC_TRANS_REMAP_RPU0A",  .addr = A_NOC_TRANS_REMAP_RPU0A,
        .rsvd = 0xfffffffe,
    },{ .name = "NOC_TRANS_REMAP_RPU1A",  .addr = A_NOC_TRANS_REMAP_RPU1A,
        .rsvd = 0xfffffffe,
    },{ .name = "NOC_TRANS_REMAP_RPU0B",  .addr = A_NOC_TRANS_REMAP_RPU0B,
        .rsvd = 0xfffffffe,
    },{ .name = "NOC_TRANS_REMAP_RPU1B",  .addr = A_NOC_TRANS_REMAP_RPU1B,
        .rsvd = 0xfffffffe,
    },{ .name = "SMID_CONFIG_RPU0A",  .addr = A_SMID_CONFIG_RPU0A,
        .rsvd = 0xffffff00,
    },{ .name = "SMID_CONFIG_RPU1A",  .addr = A_SMID_CONFIG_RPU1A,
        .rsvd = 0xffffff00,
    },{ .name = "SMID_CONFIG_RPU0B",  .addr = A_SMID_CONFIG_RPU0B,
        .rsvd = 0xffffff00,
    },{ .name = "SMID_CONFIG_RPU1B",  .addr = A_SMID_CONFIG_RPU1B,
        .rsvd = 0xffffff00,
    },{ .name = "WWDT0_CLK_SEL",  .addr = A_WWDT0_CLK_SEL,
        .rsvd = 0xfffffffc,
    },{ .name = "WWDT1_CLK_SEL",  .addr = A_WWDT1_CLK_SEL,
        .rsvd = 0xfffffffc,
    },{ .name = "SSC_SWITCH_EN",  .addr = A_SSC_SWITCH_EN,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_0",  .addr = A_ADMA0_ROUTE_CR_0,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_1",  .addr = A_ADMA0_ROUTE_CR_1,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_2",  .addr = A_ADMA0_ROUTE_CR_2,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_3",  .addr = A_ADMA0_ROUTE_CR_3,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_4",  .addr = A_ADMA0_ROUTE_CR_4,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_5",  .addr = A_ADMA0_ROUTE_CR_5,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_6",  .addr = A_ADMA0_ROUTE_CR_6,
        .rsvd = 0xfffffffe,
    },{ .name = "ADMA0_ROUTE_CR_7",  .addr = A_ADMA0_ROUTE_CR_7,
        .rsvd = 0xfffffffe,
    },{ .name = "NMU_RD_FIREWALL_ISR",  .addr = A_NMU_RD_FIREWALL_ISR,
        .rsvd = 0xffff8000,
        .w1c = 0x7fff,
        .post_write = nmu_rd_firewall_isr_postw,
    },{ .name = "NMU_RD_FIREWALL_IMR",  .addr = A_NMU_RD_FIREWALL_IMR,
        .reset = 0x7fff,
        .rsvd = 0xffff8000,
        .ro = 0x7fff,
    },{ .name = "NMU_RD_FIREWALL_IEN",  .addr = A_NMU_RD_FIREWALL_IEN,
        .rsvd = 0xffff8000,
        .pre_write = nmu_rd_firewall_ien_prew,
    },{ .name = "NMU_RD_FIREWALL_IDS",  .addr = A_NMU_RD_FIREWALL_IDS,
        .rsvd = 0xffff8000,
        .pre_write = nmu_rd_firewall_ids_prew,
    },{ .name = "NMU_WR_FIREWALL_ISR",  .addr = A_NMU_WR_FIREWALL_ISR,
        .rsvd = 0xffff8000,
        .w1c = 0x7fff,
        .post_write = nmu_wr_firewall_isr_postw,
    },{ .name = "NMU_WR_FIREWALL_IMR",  .addr = A_NMU_WR_FIREWALL_IMR,
        .reset = 0x7fff,
        .rsvd = 0xffff8000,
        .ro = 0x7fff,
    },{ .name = "NMU_WR_FIREWALL_IEN",  .addr = A_NMU_WR_FIREWALL_IEN,
        .rsvd = 0xffff8000,
        .pre_write = nmu_wr_firewall_ien_prew,
    },{ .name = "NMU_WR_FIREWALL_IDS",  .addr = A_NMU_WR_FIREWALL_IDS,
        .rsvd = 0xffff8000,
        .pre_write = nmu_wr_firewall_ids_prew,
    },{ .name = "NSU_FIREWALL_ISR",  .addr = A_NSU_FIREWALL_ISR,
        .rsvd = 0xffffffc0,
        .w1c = 0x3f,
        .post_write = nsu_firewall_isr_postw,
    },{ .name = "NSU_FIREWALL_IMR",  .addr = A_NSU_FIREWALL_IMR,
        .reset = 0x3f,
        .rsvd = 0xffffffc0,
        .ro = 0x3f,
    },{ .name = "NSU_FIREWALL_IEN",  .addr = A_NSU_FIREWALL_IEN,
        .rsvd = 0xffffffc0,
        .pre_write = nsu_firewall_ien_prew,
    },{ .name = "NSU_FIREWALL_IDS",  .addr = A_NSU_FIREWALL_IDS,
        .rsvd = 0xffffffc0,
        .pre_write = nsu_firewall_ids_prew,
    },{ .name = "ZDMA_LS_CMP_OUT_ISR",  .addr = A_ZDMA_LS_CMP_OUT_ISR,
        .rsvd = 0xffc00000,
        .w1c = 0x3fffff,
        .post_write = zdma_ls_cmp_out_isr_postw,
    },{ .name = "ZDMA_LS_CMP_OUT_IMR",  .addr = A_ZDMA_LS_CMP_OUT_IMR,
        .reset = 0x3fffff,
        .rsvd = 0xffc00000,
        .ro = 0x3fffff,
    },{ .name = "ZDMA_LS_CMP_OUT_IEN",  .addr = A_ZDMA_LS_CMP_OUT_IEN,
        .rsvd = 0xffc00000,
        .pre_write = zdma_ls_cmp_out_ien_prew,
    },{ .name = "ZDMA_LS_CMP_OUT_IDS",  .addr = A_ZDMA_LS_CMP_OUT_IDS,
        .rsvd = 0xffc00000,
        .pre_write = zdma_ls_cmp_out_ids_prew,
    },{ .name = "RPU_PCIL_ISR",  .addr = A_RPU_PCIL_ISR,
        .rsvd = 0xfffffff0,
        .w1c = 0xf,
        .post_write = rpu_pcil_isr_postw,
    },{ .name = "RPU_PCIL_IMR",  .addr = A_RPU_PCIL_IMR,
        .reset = 0xf,
        .rsvd = 0xfffffff0,
        .ro = 0xf,
    },{ .name = "RPU_PCIL_IEN",  .addr = A_RPU_PCIL_IEN,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_pcil_ien_prew,
    },{ .name = "RPU_PCIL_IDS",  .addr = A_RPU_PCIL_IDS,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_pcil_ids_prew,
    },{ .name = "ZDMA_SYNC_LS_CMP_OUT_ISR",  .addr = A_ZDMA_SYNC_LS_CMP_OUT_ISR,
        .rsvd = 0xfffc0000,
        .w1c = 0x3ffff,
        .post_write = zdma_sync_ls_cmp_out_isr_postw,
    },{ .name = "ZDMA_SYNC_LS_CMP_OUT_IMR",  .addr = A_ZDMA_SYNC_LS_CMP_OUT_IMR,
        .reset = 0x3ffff,
        .rsvd = 0xfffc0000,
        .ro = 0x3ffff,
    },{ .name = "ZDMA_SYNC_LS_CMP_OUT_IEN",  .addr = A_ZDMA_SYNC_LS_CMP_OUT_IEN,
        .rsvd = 0xfffc0000,
        .pre_write = zdma_sync_ls_cmp_out_ien_prew,
    },{ .name = "ZDMA_SYNC_LS_CMP_OUT_IDS",  .addr = A_ZDMA_SYNC_LS_CMP_OUT_IDS,
        .rsvd = 0xfffc0000,
        .pre_write = zdma_sync_ls_cmp_out_ids_prew,
    },{ .name = "RPU_CLUSTERA_ERR_ISR",  .addr = A_RPU_CLUSTERA_ERR_ISR,
        .rsvd = 0xfffffff0,
        .w1c = 0xf,
        .post_write = rpu_clustera_err_isr_postw,
    },{ .name = "RPU_CLUSTERA_ERR_IMR",  .addr = A_RPU_CLUSTERA_ERR_IMR,
        .reset = 0xf,
        .rsvd = 0xfffffff0,
        .ro = 0xf,
    },{ .name = "RPU_CLUSTERA_ERR_IEN",  .addr = A_RPU_CLUSTERA_ERR_IEN,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_clustera_err_ien_prew,
    },{ .name = "RPU_CLUSTERA_ERR_IDS",  .addr = A_RPU_CLUSTERA_ERR_IDS,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_clustera_err_ids_prew,
    },{ .name = "RPU_CLUSTERB_ERR_ISR",  .addr = A_RPU_CLUSTERB_ERR_ISR,
        .rsvd = 0xfffffff0,
        .w1c = 0xf,
        .post_write = rpu_clusterb_err_isr_postw,
    },{ .name = "RPU_CLUSTERB_ERR_IMR",  .addr = A_RPU_CLUSTERB_ERR_IMR,
        .reset = 0xf,
        .rsvd = 0xfffffff0,
        .ro = 0xf,
    },{ .name = "RPU_CLUSTERB_ERR_IEN",  .addr = A_RPU_CLUSTERB_ERR_IEN,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_clusterb_err_ien_prew,
    },{ .name = "RPU_CLUSTERB_ERR_IDS",  .addr = A_RPU_CLUSTERB_ERR_IDS,
        .rsvd = 0xfffffff0,
        .pre_write = rpu_clusterb_err_ids_prew,
    }
};

static void lpx_slcr_reset_enter(Object *obj, ResetType type)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(obj);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->regs_info); ++i) {
        register_reset(&s->regs_info[i]);
    }
}

static void lpx_slcr_reset_hold(Object *obj, ResetType type)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(obj);

    rpu_clustera_err_imr_update_irq(s);
    rpu_clusterb_err_imr_update_irq(s);
    nmu_rd_firewall_imr_update_irq(s);
    nsu_firewall_imr_update_irq(s);
    zdma_ls_cmp_out_imr_update_irq(s);
    rpu_pcil_imr_update_irq(s);
    nmu_wr_firewall_imr_update_irq(s);
    nmu_wr_firewall_imr_update_irq(s);
    zdma_sync_ls_cmp_out_imr_update_irq(s);
}

static const MemoryRegionOps lpx_slcr_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void lpx_slcr_init(Object *obj)
{
    AmdLpxSlcr *s = AMD_LPX_SLCR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    RegisterInfoArray *reg_array;

    memory_region_init(&s->iomem, obj, TYPE_AMD_LPX_SLCR, LPX_SLCR_R_MAX * 4);
    reg_array =
        register_init_block32(DEVICE(obj), lpx_slcr_regs_info,
                              ARRAY_SIZE(lpx_slcr_regs_info),
                              s->regs_info, s->regs,
                              &lpx_slcr_ops,
                              AMD_LPX_SLCR_ERR_DEBUG,
                              LPX_SLCR_R_MAX * 4);
    memory_region_add_subregion(&s->iomem,
                                0x0,
                                &reg_array->mem);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq_rpu_clustera_err_imr);
    sysbus_init_irq(sbd, &s->irq_rpu_clusterb_err_imr);
    sysbus_init_irq(sbd, &s->irq_nmu_rd_firewall_imr);
    sysbus_init_irq(sbd, &s->irq_nsu_firewall_imr);
    sysbus_init_irq(sbd, &s->irq_zdma_ls_cmp_out_imr);
    sysbus_init_irq(sbd, &s->irq_rpu_pcil_imr);
    sysbus_init_irq(sbd, &s->irq_nmu_wr_firewall_imr);
    sysbus_init_irq(sbd, &s->irq_nmu_wr_firewall_ien);
    sysbus_init_irq(sbd, &s->irq_zdma_sync_ls_cmp_out_imr);
}

static const VMStateDescription vmstate_lpx_slcr = {
    .name = TYPE_AMD_LPX_SLCR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AmdLpxSlcr, LPX_SLCR_R_MAX),
        VMSTATE_END_OF_LIST(),
    }
};

static void lpx_slcr_class_init(ObjectClass *klass, const void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_lpx_slcr;
    rc->phases.enter = lpx_slcr_reset_enter;
    rc->phases.hold = lpx_slcr_reset_hold;
}

static const TypeInfo lpx_slcr_info = {
    .name          = TYPE_AMD_LPX_SLCR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AmdLpxSlcr),
    .class_init    = lpx_slcr_class_init,
    .instance_init = lpx_slcr_init,
};

static void lpx_slcr_register_types(void)
{
    type_register_static(&lpx_slcr_info);
}

type_init(lpx_slcr_register_types)
