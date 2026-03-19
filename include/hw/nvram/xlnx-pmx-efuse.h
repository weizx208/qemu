/*
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef XLNX_PMX_EFUSE_H
#define XLNX_PMX_EFUSE_H

#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/register.h"
#include "hw/zynqmp_aes_key.h"
#include "hw/nvram/xlnx-efuse.h"

#ifndef XLNX_PMX_EFUSE_CTRL_R_MAX
#define XLNX_PMX_EFUSE_CTRL_R_MAX  (1 + (0x100 / 4))
#endif

#define TYPE_XLNX_PMX_EFUSE_CTRL  "xlnx.pmx_efuse_ctrl"
#define TYPE_XLNX_PMX_EFUSE_CACHE "xlnx.pmx_efuse_cache"
#define TYPE_XLNX_EFUSE_MAP_IF "xlnx.efuse-map-if"
#define TYPE_XLNX_EFUSE_MAP_PMX "xlnx.efuse-map-pmx"


OBJECT_DECLARE_SIMPLE_TYPE(XlnxPmxEFuseCtrl,  XLNX_PMX_EFUSE_CTRL);
OBJECT_DECLARE_SIMPLE_TYPE(XlnxPmxEFuseCache, XLNX_PMX_EFUSE_CACHE);


typedef struct XlnxEfuseMapIfClass XlnxEfuseMapIfClass;
typedef struct XlnxEfuseMapIf XlnxEfuseMapIf;
DECLARE_CLASS_CHECKERS(XlnxEfuseMapIfClass, XLNX_EFUSE_MAP_IF,
                       TYPE_XLNX_EFUSE_MAP_IF)
#define XLNX_EFUSE_MAP_IF(obj) \
    INTERFACE_CHECK(XlnxEfuseMapIf, (obj), \
                    TYPE_XLNX_EFUSE_MAP_IF)

typedef struct XlnxPmxEfuseTile {
    size_t row;        /* index into fuse[] u32 array */
    size_t byte_lane;  /* [1..4] -> byte column [0..3], 5 -> entire 32 bits row */
    size_t len;
} XlnxPmxEfuseTile;

typedef enum XlnxEfuseMapIdx {
    XLNX_EFUSE_MAP_REG_EXPOSED, /* reg exposed eFuses through efuse cache */
    XLNX_EFUSE_MAP_AES_KEY,
    XLNX_EFUSE_MAP_USER0_KEY,
    XLNX_EFUSE_MAP_USER1_KEY,
    XLNX_EFUSE_MAP_UDS,
    XLNX_EFUSE_MAP_SYSMON,
    XLNX_EFUSE_MAP_PUF,
} XlnxEfuseMapIdx;

typedef enum XlnxEfuseBitIdx {
    XLNX_EFUSE_BIT_GLITCH_DET_WR_LK,
    XLNX_EFUSE_BIT_AES_DIS,
    XLNX_EFUSE_BIT_UDS_WR_LK,
    XLNX_EFUSE_BIT_PPK0_WR_LK,
    XLNX_EFUSE_BIT_PPK1_WR_LK,
    XLNX_EFUSE_BIT_PPK2_WR_LK,
    XLNX_EFUSE_BIT_AES_CRC_LK_0,
    XLNX_EFUSE_BIT_AES_CRC_LK_1,
    XLNX_EFUSE_BIT_AES_WR_LK,
    XLNX_EFUSE_BIT_USER_KEY_0_CRC_LK_0,
    XLNX_EFUSE_BIT_USER_KEY_0_WR_LK,
    XLNX_EFUSE_BIT_USER_KEY_1_CRC_LK_0,
    XLNX_EFUSE_BIT_USER_KEY_1_WR_LK,
    XLNX_EFUSE_BIT_PUF_SYN_LK,
    XLNX_EFUSE_BIT_PUF_DIS,
    XLNX_EFUSE_BIT_BOOT_ENV_WR_LK,
    XLNX_EFUSE_BIT_GLITCH_DET_EN,
} XlnxEfuseBitIdx;

typedef enum XlnxEfuseAccessCtrl {
    XLNX_EFUSE_AC_NEVER = 0,
    XLNX_EFUSE_AC_ALWAYS,
    XLNX_EFUSE_AC_37F,
    XLNX_EFUSE_AC_TBIT0,
    XLNX_EFUSE_AC_DME,
    XLNX_EFUSE_AC_DNA,
    XLNX_EFUSE_AC_FACTORY,
    XLNX_EFUSE_AC_RFSOC,
    XLNX_EFUSE_AC_FACTORY_5EC,
    XLNX_EFUSE_AC_FACTORY_ROW0,
    XLNX_EFUSE_AC_TBIT0_58C,
    XLNX_EFUSE_AC_TBIT0_58E,
    XLNX_EFUSE_AC_TBIT0_58F,
    XLNX_EFUSE_AC_TBIT0_5A8,
    XLNX_EFUSE_AC_TBIT0_5AB,
    XLNX_EFUSE_AC_TBIT0_5AD,
    XLNX_EFUSE_AC_TBIT0_5AF,
    XLNX_EFUSE_AC_TBIT0_5C8,
    XLNX_EFUSE_AC_TBIT0_REVOKE,
    XLNX_EFUSE_AC_TBIT1_5C8,
} XlnxEfuseAccessCtrl;

typedef enum XlnxEfuseAccessCtrlIdx {
    XLNX_EFUSE_AC_WR_ONLY,
    XLNX_EFUSE_AC_RD_ONLY,
} XlnxEfuseAccessCtrlIdx;

struct XlnxEfuseMapIfClass {
    InterfaceClass parent_class;

    const XlnxPmxEfuseTile * (*get_mapping)(XlnxEfuseMapIf *iface,
                                            XlnxEfuseMapIdx idx,
                                            size_t *len);
    size_t (*get_bit_idx)(XlnxEfuseMapIf *iface,
                          XlnxEfuseBitIdx idx);
};

static inline const XlnxPmxEfuseTile *
xlnx_efuse_map_get(XlnxEfuseMapIf *iface,
                   XlnxEfuseMapIdx idx,
                   size_t *len)
{
    XlnxEfuseMapIfClass *c = XLNX_EFUSE_MAP_IF_GET_CLASS(iface);

    return c->get_mapping(iface, idx, len);
}

static inline size_t xlnx_efuse_map_get_bit_idx(XlnxEfuseMapIf *iface,
                                                XlnxEfuseBitIdx idx)
{
    XlnxEfuseMapIfClass *c = XLNX_EFUSE_MAP_IF_GET_CLASS(iface);

    return c->get_bit_idx(iface, idx);
}

static inline bool xlnx_efuse_map_get_bit(XlnxEfuseMapIf *iface,
                                          XlnxEFuse *efuse,
                                          XlnxEfuseBitIdx idx)
{
    size_t b_idx = xlnx_efuse_map_get_bit_idx(iface, idx);

    return xlnx_efuse_get_bit(efuse, b_idx);
}

typedef struct XlnxPmxEFuseCtrl {
    SysBusDevice parent_obj;
    qemu_irq irq_efuse_imr;

    XlnxEFuse *efuse;
    ZynqMPAESKeySink *aes_key_sink;
    ZynqMPAESKeySink *usr_key0_sink;
    ZynqMPAESKeySink *usr_key1_sink;
    XlnxEfuseMapIf *mapping;

    bool ac_dme;
    bool ac_dna;
    bool ac_factory;
    bool ac_rfsoc;
    bool ac_row0;

    uint32_t regs[XLNX_PMX_EFUSE_CTRL_R_MAX];
    RegisterInfo regs_info[XLNX_PMX_EFUSE_CTRL_R_MAX];
} XlnxPmxEFuseCtrl;

typedef struct XlnxPmxEFuseCache {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    XlnxEFuse *efuse;
} XlnxPmxEFuseCache;

/**
 * Retrieve, from the @efuse storage, the 32-bit word that would be
 * visible at byte-offset (@bit / 8) through the XLNX_PMX_EFUSE_CACHE
 * device.
 *
 * For example, CRC for all data in the @efuse storage will be
 * returned when given @bit == (8 * 0x23c + 1).
 *
 * If @denied is not NULL, it is set to true if all bits of the requested
 * word is write-only; it is set to false if at least 1 bit is readable.
 *
 * Return: 0 if @denied is set (or would have been set) to true.
 */
static inline uint32_t xlnx_pmx_efuse_read_row(XlnxEFuse *efuse,
                                               uint32_t bit, bool *denied)
{
    return xlnx_efuse_get_u32(efuse, bit, denied);
}

#endif
