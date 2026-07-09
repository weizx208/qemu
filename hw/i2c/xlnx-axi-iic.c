/*
 * Xilinx AXI IIC stub model
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/registerfields.h"
#include "hw/i2c/xlnx-axi-iic.h"
#include "trace.h"

REG32(GIE, 0x1c)
REG32(ISR, 0x20)
REG32(IER, 0x28)
REG32(SOFTR, 0x40)
REG32(CR, 0x100)
REG32(SR, 0x104)
    FIELD(SR, ABGC, 0, 1)
    FIELD(SR, AAS, 1, 1)
    FIELD(SR, BB, 2, 1)
    FIELD(SR, SRW, 3, 1)
    FIELD(SR, TX_FIFO_FULL, 4, 1)
    FIELD(SR, RX_FIFO_FULL, 5, 1)
    FIELD(SR, TX_FIFO_EMPTY, 6, 1)
    FIELD(SR, RX_FIFO_EMPTY, 7, 1)
REG32(TX_FIFO, 0x108)
REG32(RX_FIFO, 0x10c)
REG32(ADR, 0x110)
REG32(TX_FIFO_OCY, 0x114)
REG32(RX_FIFO_OCY, 0x118)
REG32(TEN_ADR, 0x11c)
REG32(RX_FIFO_PIRQ, 0x120)
REG32(GPO, 0x124)
REG32(TSUSTA, 0x128)
REG32(TSUSTO, 0x12c)
REG32(THDSTA, 0x130)
REG32(TSUDAT, 0x134)
REG32(TBUF, 0x138)
REG32(THIGH, 0x13c)
REG32(TLOW, 0x140)
REG32(THDDAT, 0x144)

const char *XILINX_AXI_IIC_REG_NAME[] = {
    [R_GIE] = "GIE",
    [R_ISR] = "ISR",
    [R_IER] = "IER",
    [R_SOFTR] = "SOFTR",
    [R_CR] = "CR",
    [R_SR] = "SR",
    [R_TX_FIFO] = "TX_FIFO",
    [R_RX_FIFO] = "RX_FIFO",
    [R_ADR] = "ADR",
    [R_TX_FIFO_OCY] = "TX_FIFO_OCY",
    [R_RX_FIFO_OCY] = "RX_FIFO_OCY",
    [R_TEN_ADR] = "TEN_ADR",
    [R_RX_FIFO_PIRQ] = "RX_FIFO_PIRQ",
    [R_GPO] = "GPO",
    [R_TSUSTA] = "TSUSTA",
    [R_TSUSTO] = "TSUSTO",
    [R_THDSTA] = "THDSTA",
    [R_TSUDAT] = "TSUDAT",
    [R_TBUF] = "TBUF",
    [R_THIGH] = "THIGH",
    [R_TLOW] = "TLOW",
    [R_THDDAT] = "THDDAT",
};

static uint64_t xilinx_axi_iic_read(void *opaque, hwaddr addr,
                                    unsigned int size)
{
    uint32_t ret;

    switch (addr) {
    case A_SR:
        ret = FIELD_DP32(0, SR, RX_FIFO_EMPTY, 1);
        ret = FIELD_DP32(ret, SR, TX_FIFO_EMPTY, 1);
        break;

    default:
        ret = 0;
    }

    trace_xilinx_axi_iic_read(XILINX_AXI_IIC_REG_NAME[addr / 4],
                              addr, size, ret);
    return ret;
}

static void xilinx_axi_iic_write(void *opaque, hwaddr addr, uint64_t value,
                                 unsigned int size)
{
    trace_xilinx_axi_iic_write(XILINX_AXI_IIC_REG_NAME[addr / 4],
                               addr, size, value);
}

static const MemoryRegionOps xilinx_axi_iic_ops = {
    .read = xilinx_axi_iic_read,
    .write = xilinx_axi_iic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void xilinx_axi_iic_realize(DeviceState *dev, Error **errp)
{
    XilinxAxiIICState *s = XILINX_AXI_IIC(dev);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    s->bus = i2c_init_bus(dev, "i2c");
}

static void xilinx_axi_iic_init(Object *obj)
{
    XilinxAxiIICState *s = XILINX_AXI_IIC(obj);

    memory_region_init_io(&s->iomem, obj, &xilinx_axi_iic_ops, s,
                          TYPE_XILINX_AXI_IIC, XILINX_AXI_IIC_MMIO_LEN);
}

static void xilinx_axi_iic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = xilinx_axi_iic_realize;
}

static const TypeInfo xilinx_axi_iic_info = {
    .name = TYPE_XILINX_AXI_IIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(XilinxAxiIICState),
    .instance_init = xilinx_axi_iic_init,
    .class_init = xilinx_axi_iic_class_init,
};

static void xilinx_axi_iic_register_types(void)
{
    type_register_static(&xilinx_axi_iic_info);
}

type_init(xilinx_axi_iic_register_types)
