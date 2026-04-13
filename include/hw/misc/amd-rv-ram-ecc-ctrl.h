/*
 * QEMU model of the RV_RAM_ECC_CTRL MB_RAM_ECC_CTRL
 *
 * Copyright (c) 2026, Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_AMD_RV_RAM_ECC_CTRL_H
#define HW_MISC_AMD_RV_RAM_ECC_CTRL_H

#include "hw/sysbus.h"
#include "hw/register.h"

#define TYPE_AMD_RV_RAM_ECC_CTRL "amd.rv_ram_ecc_ctrl"

#define AMD_RV_RAM_ECC_CTRL(obj) \
     OBJECT_CHECK(RvRamEccCtrl, (obj), TYPE_AMD_RV_RAM_ECC_CTRL)

#define RV_RAM_ECC_CTRL_R_MAX (0x380 / 4 + 1)

typedef struct RvRamEccCtrl {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t regs[RV_RAM_ECC_CTRL_R_MAX];
    RegisterInfo regs_info[RV_RAM_ECC_CTRL_R_MAX];
} RvRamEccCtrl;

#endif
