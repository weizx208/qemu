/*
 * Xilinx AXI IIC stub model
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_I2C_XLNX_AXI_IIC_H
#define HW_I2C_XLNX_AXI_IIC_H

#include "hw/sysbus.h"
#include "hw/i2c/i2c.h"

#define TYPE_XILINX_AXI_IIC "xlnx-axi-iic"
OBJECT_DECLARE_SIMPLE_TYPE(XilinxAxiIICState, XILINX_AXI_IIC)

#define XILINX_AXI_IIC_MMIO_LEN 0x148

struct XilinxAxiIICState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    I2CBus *bus;
};

#endif
