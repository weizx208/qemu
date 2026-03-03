/*
 * QEMU model of the PmcSha3V2 Secure Hash Algorithm
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "hw/sysbus.h"
#include "hw/register.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/irq.h"
#include "hw/hw.h"
#include "hw/crypto/xlnx-sha3-common.h"

#ifndef XILINX_PMC_SHA3_V2_ERR_DEBUG
#define XILINX_PMC_SHA3_V2_ERR_DEBUG 0
#endif

#define TYPE_XILINX_PMC_SHA3_V2 "xlnx.pmc_sha3_v2.1"

#define XILINX_PMC_SHA3_V2(obj) \
     OBJECT_CHECK(PmcSha3V2, (obj), TYPE_XILINX_PMC_SHA3_V2)

#define PMC_SHA3_V2_MAX_DIGEST_LEN (42 * 4)

REG32(SHA_START, 0x0)
    FIELD(SHA_START, VALUE, 0, 1)
REG32(SHA_RESET, 0x4)
    FIELD(SHA_RESET, VALUE, 0, 1)
REG32(SHA_DONE, 0x8)
    FIELD(SHA_DONE, VALUE, 0, 1)
REG32(SHA_NEXT_XOF, 0xc)
    FIELD(SHA_NEXT_XOF, VALUE, 0, 1)
REG32(SHA_DIGEST_0, 0x10)
REG32(SHA_DIGEST_1, 0x14)
REG32(SHA_DIGEST_2, 0x18)
REG32(SHA_DIGEST_3, 0x1c)
REG32(SHA_DIGEST_4, 0x20)
REG32(SHA_DIGEST_5, 0x24)
REG32(SHA_DIGEST_6, 0x28)
REG32(SHA_DIGEST_7, 0x2c)
REG32(SHA_DIGEST_8, 0x30)
REG32(SHA_DIGEST_9, 0x34)
REG32(SHA_DIGEST_10, 0x38)
REG32(SHA_DIGEST_11, 0x3c)
REG32(SHA_DIGEST_12, 0x40)
REG32(SHA_DIGEST_13, 0x44)
REG32(SHA_DIGEST_14, 0x48)
REG32(SHA_DIGEST_15, 0x4c)
REG32(SHA_DIGEST_16, 0x50)
REG32(SHA_DIGEST_17, 0x54)
REG32(SHA_DIGEST_18, 0x58)
REG32(SHA_DIGEST_19, 0x5c)
REG32(SHA_DIGEST_20, 0x60)
REG32(SHA_DIGEST_21, 0x64)
REG32(SHA_DIGEST_22, 0x68)
REG32(SHA_DIGEST_23, 0x6c)
REG32(SHA_DIGEST_24, 0x70)
REG32(SHA_DIGEST_25, 0x74)
REG32(SHA_DIGEST_26, 0x78)
REG32(SHA_DIGEST_27, 0x7c)
REG32(SHA_DIGEST_28, 0x80)
REG32(SHA_DIGEST_29, 0x84)
REG32(SHA_DIGEST_30, 0x88)
REG32(SHA_DIGEST_31, 0x8c)
REG32(SHA_DIGEST_32, 0x90)
REG32(SHA_DIGEST_33, 0x94)
REG32(SHA_DIGEST_34, 0x98)
REG32(SHA_DIGEST_35, 0x9c)
REG32(SHA_DIGEST_36, 0xa0)
REG32(SHA_DIGEST_37, 0xa4)
REG32(SHA_DIGEST_38, 0xa8)
REG32(SHA_DIGEST_39, 0xac)
REG32(SHA_DIGEST_40, 0xb0)
REG32(SHA_DIGEST_41, 0xb4)
REG32(SHA_MODE, 0xc0)
    FIELD(SHA_MODE, VALUE, 0, 3)
REG32(SHA_AUTO_PADDING, 0xc4)
    FIELD(SHA_AUTO_PADDING, ENABLE, 0, 1)
REG32(SHA_CHAIN, 0xc8)
    FIELD(SHA_CHAIN, STEPS, 16, 8)
    FIELD(SHA_CHAIN, START, 0, 8)
REG32(SHA_ISR, 0xd4)
    FIELD(SHA_ISR, DONE, 0, 1)
REG32(SHA_IMR, 0xd8)
    FIELD(SHA_IMR, DONE, 0, 1)
REG32(SHA_IER, 0xdc)
    FIELD(SHA_IER, DONE, 0, 1)
REG32(SHA_IDR, 0xe0)
    FIELD(SHA_IDR, DONE, 0, 1)
REG32(SHA_ITR, 0xe4)
    FIELD(SHA_ITR, DONE, 0, 1)
REG32(SHA_VERSION, 0xf8)
    FIELD(SHA_VERSION, VALUE, 0, 32)

#define PMC_SHA3_V2_R_MAX (R_SHA_VERSION + 1)

typedef struct PmcSha3V2 {
    XlnxSha3Common parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_sha_imr;
    bool reseting;

    uint32_t regs[PMC_SHA3_V2_R_MAX];
    RegisterInfo regs_info[PMC_SHA3_V2_R_MAX];
} PmcSha3V2;

static void sha_imr_update_irq(PmcSha3V2 *s)
{
    bool pending = s->regs[R_SHA_ISR] & ~s->regs[R_SHA_IMR];
    qemu_set_irq(s->irq_sha_imr, pending);
}

static void sha_isr_postw(RegisterInfo *reg, uint64_t val64)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    sha_imr_update_irq(s);
}

static uint64_t sha_ier_prew(RegisterInfo *reg, uint64_t val64)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    uint32_t val = val64;

    s->regs[R_SHA_IMR] &= ~val;
    sha_imr_update_irq(s);
    return 0;
}

static uint64_t sha_idr_prew(RegisterInfo *reg, uint64_t val64)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    uint32_t val = val64;

    s->regs[R_SHA_IMR] |= val;
    sha_imr_update_irq(s);
    return 0;
}

static uint64_t sha_itr_prew(RegisterInfo *reg, uint64_t val64)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    uint32_t val = val64;

    s->regs[R_SHA_ISR] |= val;
    sha_imr_update_irq(s);
    return 0;
}

static void pmc_sha3_v2_reset_postw(RegisterInfo *reg, uint64_t value)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    XlnxSha3Common *common = XLNX_SHA3_COMMON(s);

    /* Pass the reset signal to the SHA3 common block.  */
    xlnx_sha3_common_reset(common, ARRAY_FIELD_EX32(s->regs, SHA_RESET, VALUE));

    if (ARRAY_FIELD_EX32(s->regs, SHA_RESET, VALUE)) {
        /* Puts the device in reset mode.  */
        s->reseting = true;
        resettable_reset(OBJECT(s), RESET_TYPE_COLD);
    } else if (s->reseting) {
        /* 1 -> 0 release from reset mode.  */
        s->reseting = false;
    }
}

static void pmc_sha3_v2_start_postw(RegisterInfo *reg, uint64_t value)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    XlnxSha3Common *common = XLNX_SHA3_COMMON(s);

    if (s->reseting) {
        /* Don't do anything during reset.  */
        return;
    }

    if (!ARRAY_FIELD_EX32(s->regs, SHA_START, VALUE)) {
        /* Writing zero to that register doesn't have any effect.  */
        return;
    }

    /* Start bit is self clearing.  */
    ARRAY_FIELD_DP32(s->regs, SHA_START, VALUE, false);

    /*
     * Drop the SHA_DONE bit, driver will need to wait for this one to be set
     * before reading the digest.
     */
    ARRAY_FIELD_DP32(s->regs, SHA_DONE, VALUE, false);

    /*
     * Start the SHA3 common block, so it will accept DMA streams and start
     * computing hashes.
     */
    xlnx_sha3_common_start(common);
}

static void pmc_sha3_v2_next_xof_postw(RegisterInfo *reg, uint64_t value)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(reg->opaque);
    XlnxSha3Common *common = XLNX_SHA3_COMMON(s);

    if (!ARRAY_FIELD_EX32(s->regs, SHA_NEXT_XOF, VALUE)) {
        return;
    }

    xlnx_sha3_common_next_xof(XLNX_SHA3_COMMON(s));
    xlnx_sha3_common_update_digest(common);
    /*
     * XXX: The SHA_NEXT_XOF bit auto-clear, but it is unsure how the user is
     *      informed of the availability of new digest.
     */
    ARRAY_FIELD_DP32(s->regs, SHA_NEXT_XOF, VALUE, false);
}

static const RegisterAccessInfo pmc_sha3_v2_regs_info[] = {
    {   .name = "SHA_START",  .addr = A_SHA_START,
        .post_write = pmc_sha3_v2_start_postw,
    },{ .name = "SHA_RESET",  .addr = A_SHA_RESET,
        .reset = 0x1,
        .post_write = pmc_sha3_v2_reset_postw,
    },{ .name = "SHA_DONE",  .addr = A_SHA_DONE,
        .ro = 0x1,
    },{ .name = "SHA_NEXT_XOF",  .addr = A_SHA_NEXT_XOF,
        .post_write = pmc_sha3_v2_next_xof_postw,
    },{ .name = "SHA_DIGEST_0",  .addr = A_SHA_DIGEST_0,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_1",  .addr = A_SHA_DIGEST_1,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_2",  .addr = A_SHA_DIGEST_2,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_3",  .addr = A_SHA_DIGEST_3,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_4",  .addr = A_SHA_DIGEST_4,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_5",  .addr = A_SHA_DIGEST_5,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_6",  .addr = A_SHA_DIGEST_6,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_7",  .addr = A_SHA_DIGEST_7,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_8",  .addr = A_SHA_DIGEST_8,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_9",  .addr = A_SHA_DIGEST_9,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_10",  .addr = A_SHA_DIGEST_10,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_11",  .addr = A_SHA_DIGEST_11,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_12",  .addr = A_SHA_DIGEST_12,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_13",  .addr = A_SHA_DIGEST_13,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_14",  .addr = A_SHA_DIGEST_14,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_15",  .addr = A_SHA_DIGEST_15,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_16",  .addr = A_SHA_DIGEST_16,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_17",  .addr = A_SHA_DIGEST_17,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_18",  .addr = A_SHA_DIGEST_18,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_19",  .addr = A_SHA_DIGEST_19,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_20",  .addr = A_SHA_DIGEST_20,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_21",  .addr = A_SHA_DIGEST_21,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_22",  .addr = A_SHA_DIGEST_22,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_23",  .addr = A_SHA_DIGEST_23,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_24",  .addr = A_SHA_DIGEST_24,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_25",  .addr = A_SHA_DIGEST_25,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_26",  .addr = A_SHA_DIGEST_26,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_27",  .addr = A_SHA_DIGEST_27,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_28",  .addr = A_SHA_DIGEST_28,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_29",  .addr = A_SHA_DIGEST_29,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_30",  .addr = A_SHA_DIGEST_30,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_31",  .addr = A_SHA_DIGEST_31,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_32",  .addr = A_SHA_DIGEST_32,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_33",  .addr = A_SHA_DIGEST_33,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_34",  .addr = A_SHA_DIGEST_34,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_35",  .addr = A_SHA_DIGEST_35,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_36",  .addr = A_SHA_DIGEST_36,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_37",  .addr = A_SHA_DIGEST_37,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_38",  .addr = A_SHA_DIGEST_38,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_39",  .addr = A_SHA_DIGEST_39,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_40",  .addr = A_SHA_DIGEST_40,
        .ro = 0xffffffff,
    },{ .name = "SHA_DIGEST_41",  .addr = A_SHA_DIGEST_41,
        .ro = 0xffffffff,
    },{ .name = "SHA_MODE",  .addr = A_SHA_MODE,
    },{ .name = "SHA_AUTO_PADDING",  .addr = A_SHA_AUTO_PADDING,
    },{ .name = "SHA_CHAIN",  .addr = A_SHA_CHAIN,
    },{ .name = "SHA_ISR",  .addr = A_SHA_ISR,
        .w1c = 0x1,
        .post_write = sha_isr_postw,
    },{ .name = "SHA_IMR",  .addr = A_SHA_IMR,
        .reset = 0x1,
        .ro = 0x1,
    },{ .name = "SHA_IER",  .addr = A_SHA_IER,
        .pre_write = sha_ier_prew,
    },{ .name = "SHA_IDR",  .addr = A_SHA_IDR,
        .pre_write = sha_idr_prew,
    },{ .name = "SHA_ITR",  .addr = A_SHA_ITR,
        .pre_write = sha_itr_prew,
    },{ .name = "SHA_VERSION",  .addr = A_SHA_VERSION,
        .ro = 0xffffffff,
    }
};

static bool pmc_sha3_v2_autopadding_enabled(XlnxSha3Common *common)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);

    return (ARRAY_FIELD_EX32(s->regs, SHA_AUTO_PADDING, ENABLE) != 0);
}

static void pmc_sha3_v2_handle_end_of_packet(XlnxSha3Common *common)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);
    /* The SHA3 model got an end of packet.  */
    ARRAY_FIELD_DP32(s->regs, SHA_DONE, VALUE, true);
    ARRAY_FIELD_DP32(s->regs, SHA_ISR, DONE, true);
    sha_imr_update_irq(s);
}

static uint8_t pmc_sha3_v2_get_chain_start(XlnxSha3Common *common)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);

    return (uint8_t) ARRAY_FIELD_EX32(s->regs, SHA_CHAIN, START);
}

static uint8_t pmc_sha3_v2_get_chain_steps(XlnxSha3Common *common)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);

    return (uint8_t) ARRAY_FIELD_EX32(s->regs, SHA_CHAIN, STEPS);
}

static void pmc_sha3_v2_write_digest(XlnxSha3Common *common, uint32_t *digest,
                                     size_t size)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);

    /* Sanity check that the digest fits in the destination.  */
    assert(size <= PMC_SHA3_V2_MAX_DIGEST_LEN);

    memcpy(&s->regs[R_SHA_DIGEST_0], digest, size > 34 * 4 ?
                                             34 * 4 : size);
    if (size > 34 * 4) {
        memcpy(&s->regs[R_SHA_DIGEST_34], &digest[34], size - (34 * 4));
    }
}

static XlnxSha3CommonAlg pmc_sha3_v2_get_algorithm(XlnxSha3Common *common)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(common);

    switch (ARRAY_FIELD_EX32(s->regs, SHA_MODE, VALUE)) {
    case 0:
        return SHA_MODE_256;
    case 1:
        return SHA_MODE_384;
    case 2:
        return SHA_MODE_512;
    case 3:
        return SHA_MODE_SHAKE128;
    case 4:
        return SHA_MODE_SHAKE256;
    case 5:
        return SHA_MODE_SHAKE256_LMS_OTS_CHAIN;
    case 6:
        return SHA_MODE_SHAKE256_SLH_DSA_CHAIN;
    default:
        return SHA_MODE_UNIMPLEMENTED;
    }
}

static void pmc_sha3_v2_reset_enter(Object *obj, ResetType type)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(obj);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->regs_info); ++i) {
        /*
         * Reset from the bus, will as a side effect call
         * pmc_sha3_v2_reset_postw.  Avoid infinite recursion here.
         */
        if (!s->reseting || (i != R_SHA_RESET)) {
            register_reset(&s->regs_info[i]);
        } else {
            s->regs[R_SHA_RESET] = 0x1;
        }
    }
    ARRAY_FIELD_DP32(s->regs, SHA_VERSION, VALUE, 0x20001);
}

static void pmc_sha3_v2_reset_hold(Object *obj)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(obj);

    sha_imr_update_irq(s);
}

static const MemoryRegionOps pmc_sha3_v2_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void pmc_sha3_v2_init(Object *obj)
{
    PmcSha3V2 *s = XILINX_PMC_SHA3_V2(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    RegisterInfoArray *reg_array;

    memory_region_init(&s->iomem, obj, TYPE_XILINX_PMC_SHA3_V2,
                       PMC_SHA3_V2_R_MAX * 4);
    reg_array =
        register_init_block32(DEVICE(obj), pmc_sha3_v2_regs_info,
                              ARRAY_SIZE(pmc_sha3_v2_regs_info),
                              s->regs_info, s->regs,
                              &pmc_sha3_v2_ops,
                              XILINX_PMC_SHA3_V2_ERR_DEBUG,
                              PMC_SHA3_V2_R_MAX * 4);
    memory_region_add_subregion(&s->iomem,
                                0x0,
                                &reg_array->mem);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq_sha_imr);
}

static const VMStateDescription vmstate_pmc_sha3_v2 = {
    .name = TYPE_XILINX_PMC_SHA3_V2,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, PmcSha3V2, PMC_SHA3_V2_R_MAX),
        VMSTATE_END_OF_LIST(),
    }
};

static void pmc_sha3_v2_class_init(ObjectClass *klass, void *data)
{
    XlnxSha3CommonClass *cc = XLNX_SHA3_COMMON_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_pmc_sha3_v2;
    rc->phases.enter = pmc_sha3_v2_reset_enter;
    rc->phases.hold = pmc_sha3_v2_reset_hold;
    cc->is_autopadding_enabled = pmc_sha3_v2_autopadding_enabled;
    cc->end_of_packet_notifier = pmc_sha3_v2_handle_end_of_packet;
    cc->get_chain_start = pmc_sha3_v2_get_chain_start;
    cc->get_chain_steps = pmc_sha3_v2_get_chain_steps;
    cc->write_digest = pmc_sha3_v2_write_digest;
    cc->get_algorithm = pmc_sha3_v2_get_algorithm;
}

static const TypeInfo pmc_sha3_v2_info = {
    .name          = TYPE_XILINX_PMC_SHA3_V2,
    .parent        = TYPE_XLNX_SHA3_COMMON,
    .instance_size = sizeof(PmcSha3V2),
    .class_init    = pmc_sha3_v2_class_init,
    .instance_init = pmc_sha3_v2_init,
};

static void pmc_sha3_v2_register_types(void)
{
    type_register_static(&pmc_sha3_v2_info);
}

type_init(pmc_sha3_v2_register_types)
