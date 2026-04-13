/*
 * QEMU model of the RV_RAM_ECC_CTRL MB_RAM_ECC_CTRL
 *
 * Copyright (c) 2026, Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/amd-rv-ram-ecc-ctrl.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "migration/vmstate.h"

#ifndef AMD_RV_RAM_ECC_CTRL_ERR_DEBUG
#define AMD_RV_RAM_ECC_CTRL_ERR_DEBUG 0
#endif

REG32(STATUS, 0x0)
    FIELD(STATUS, CE, 1, 1)
    FIELD(STATUS, UE, 0, 1)
REG32(EN_IRQ, 0x4)
    FIELD(EN_IRQ, CE_IRQ_EN, 1, 1)
    FIELD(EN_IRQ, UE_IRQ_EN, 0, 1)
REG32(ONOFF, 0x8)
    FIELD(ONOFF, ECC_EN, 0, 1)
REG32(CE_CNT, 0xc)
    FIELD(CE_CNT, CE_CNT, 0, 16)
REG32(CE_FFD, 0x100)
REG32(CE_FFE, 0x180)
    FIELD(CE_FFE, CE_FIRST_FAIL_ECC, 0, 7)
REG32(CE_FFA, 0x1c0)
REG32(UE_FFD, 0x200)
REG32(UE_FFE, 0x280)
    FIELD(UE_FFE, UE_FIRST_FAIL_ECC, 0, 7)
REG32(UE_FFA, 0x2c0)
REG32(FI_D, 0x300)
REG32(FI_ECC, 0x380)
    FIELD(FI_ECC, ECC_FAULT_INJECT, 0, 7)

static const RegisterAccessInfo rv_ram_ecc_ctrl_regs_info[] = {
    {   .name = "STATUS",  .addr = A_STATUS,
        .rsvd = 0xfffffffc,
    },{ .name = "EN_IRQ",  .addr = A_EN_IRQ,
        .rsvd = 0xfffffffc,
    },{ .name = "ONOFF",  .addr = A_ONOFF,
        .reset = 0x1,
        .rsvd = 0xfffffffe,
    },{ .name = "CE_CNT",  .addr = A_CE_CNT,
        .rsvd = 0xffff0000,
    },{ .name = "CE_FFD",  .addr = A_CE_FFD,
        .ro = 0xffffffff,
    },{ .name = "CE_FFE",  .addr = A_CE_FFE,
        .rsvd = 0xffffff80,
        .ro = 0xffffffff,
    },{ .name = "CE_FFA",  .addr = A_CE_FFA,
        .ro = 0xffffffff,
    },{ .name = "UE_FFD",  .addr = A_UE_FFD,
        .ro = 0xffffffff,
    },{ .name = "UE_FFE",  .addr = A_UE_FFE,
        .rsvd = 0xffffff80,
        .ro = 0xffffffff,
    },{ .name = "UE_FFA",  .addr = A_UE_FFA,
        .ro = 0xffffffff,
    },{ .name = "FI_D",  .addr = A_FI_D,
    },{ .name = "FI_ECC",  .addr = A_FI_ECC,
        .rsvd = 0xffffff80,
    }
};

static void rv_ram_ecc_ctrl_reset_enter(Object *obj, ResetType type)
{
    RvRamEccCtrl *s = AMD_RV_RAM_ECC_CTRL(obj);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->regs_info); ++i) {
        register_reset(&s->regs_info[i]);
    }
}

static const MemoryRegionOps rv_ram_ecc_ctrl_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void rv_ram_ecc_ctrl_init(Object *obj)
{
    RvRamEccCtrl *s = AMD_RV_RAM_ECC_CTRL(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    RegisterInfoArray *reg_array;

    memory_region_init(&s->iomem, obj, TYPE_AMD_RV_RAM_ECC_CTRL,
                       RV_RAM_ECC_CTRL_R_MAX * 4);
    reg_array =
        register_init_block32(DEVICE(obj), rv_ram_ecc_ctrl_regs_info,
                              ARRAY_SIZE(rv_ram_ecc_ctrl_regs_info),
                              s->regs_info, s->regs,
                              &rv_ram_ecc_ctrl_ops,
                              AMD_RV_RAM_ECC_CTRL_ERR_DEBUG,
                              RV_RAM_ECC_CTRL_R_MAX * 4);
    memory_region_add_subregion(&s->iomem,
                                0x0,
                                &reg_array->mem);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_rv_ram_ecc_ctrl = {
    .name = TYPE_AMD_RV_RAM_ECC_CTRL,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, RvRamEccCtrl, RV_RAM_ECC_CTRL_R_MAX),
        VMSTATE_END_OF_LIST(),
    }
};

static void rv_ram_ecc_ctrl_class_init(ObjectClass *klass, void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_rv_ram_ecc_ctrl;
    rc->phases.enter = rv_ram_ecc_ctrl_reset_enter;
}

static const TypeInfo rv_ram_ecc_ctrl_info = {
    .name          = TYPE_AMD_RV_RAM_ECC_CTRL,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RvRamEccCtrl),
    .class_init    = rv_ram_ecc_ctrl_class_init,
    .instance_init = rv_ram_ecc_ctrl_init,
};

static void rv_ram_ecc_ctrl_register_types(void)
{
    type_register_static(&rv_ram_ecc_ctrl_info);
}

type_init(rv_ram_ecc_ctrl_register_types)
