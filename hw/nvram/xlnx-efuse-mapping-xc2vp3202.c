/*
 * Xilinx XC2VP3202 eFuse mapping
 *
 * Copyright Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "qemu/osdep.h"
#include "hw/nvram/xlnx-pmx-efuse.h"

REG32(STATUS, 0x8)
    FIELD(STATUS, UDS_DICE_CRC_PASS, 13, 1)
    FIELD(STATUS, UDS_DICE_CRC_DONE, 12, 1)
    FIELD(STATUS, AES_USER_KEY_1_CRC_PASS, 11, 1)
    FIELD(STATUS, AES_USER_KEY_1_CRC_DONE, 10, 1)
    FIELD(STATUS, AES_USER_KEY_0_CRC_PASS, 9, 1)
    FIELD(STATUS, AES_USER_KEY_0_CRC_DONE, 8, 1)
    FIELD(STATUS, AES_CRC_PASS, 7, 1)
    FIELD(STATUS, AES_CRC_DONE, 6, 1)
    FIELD(STATUS, CACHE_DONE, 5, 1)
    FIELD(STATUS, CACHE_LOAD, 4, 1)
    FIELD(STATUS, EFUSE_2_TBIT, 2, 1)
    FIELD(STATUS, EFUSE_1_TBIT, 1, 1)
    FIELD(STATUS, EFUSE_0_TBIT, 0, 1)
REG32(EFUSE_PGM_LOCK, 0x44)
    FIELD(EFUSE_PGM_LOCK, REVOCATION_ID_LOCK, 0, 1)

#include "xlnx-xc2vp3202-efuse-tile.c.inc"

OBJECT_DECLARE_SIMPLE_TYPE(XlnxEfuseMapXc2vp3202State, XLNX_EFUSE_MAP_XC2VP3202)

struct XlnxEfuseMapXc2vp3202State {
    Object parent_obj;
};

/* Bits readable as 32-bit words through the pmx-efuse-cache */
static const XlnxPmxEfuseTile xc2vp3202_efuse_u32[] = {
    EFUSE_U32_TILES,
};

/*
 * Uncached 64-bit sysmon data u8 tiles (i.e., each tile has only 8 bits).
 */
static const XlnxPmxEfuseTile pmx_efuse_u8_sysmon_rd64[] = {
    EFUSE_U8_TILES_SYSMON_RD64,
};

/*
 * Write-only u8 arrays.
 *
 * pmx-efuse-ctrl can, and only can, report their calculated CRC.
 */
static const XlnxPmxEfuseTile xc2vp3202_efuse_u8_aes_key[] = {
    EFUSE_U8_TILES_AES_KEY
};

static const XlnxPmxEfuseTile xc2vp3202_efuse_u8_user0_key[] = {
    EFUSE_U8_TILES_USER0_KEY
};

static const XlnxPmxEfuseTile xc2vp3202_efuse_u8_user1_key[] = {
    EFUSE_U8_TILES_USER1_KEY
};

static const XlnxPmxEfuseTile xc2vp3202_efuse_u8_uds[] = {
    EFUSE_U8_TILES_UDS
};

static const XlnxPmxEfuseTile xc2vp3202_efuse_puf[] = {
    EFUSE_U32_TILES_PUF
};

static const uint8_t xc2vp3202_efuse_ac_wr_only[] = {
    EFUSE_ACL1_WR_ONLY
};

static const uint8_t xc2vp3202_efuse_ac_rd_only[] = {
    EFUSE_ACL1_RD_ONLY
};

static const XlnxPmxEfuseTile *xc2vp3202_efuse_mapping_get(XlnxEfuseMapIf *iface,
                                                         XlnxEfuseMapIdx idx,
                                                         size_t *len)
{
    switch (idx) {
    case XLNX_EFUSE_MAP_REG_EXPOSED:
        *len = ARRAY_SIZE(xc2vp3202_efuse_u32);
        return xc2vp3202_efuse_u32;

    case XLNX_EFUSE_MAP_AES_KEY:
        *len = ARRAY_SIZE(xc2vp3202_efuse_u8_aes_key);
        return xc2vp3202_efuse_u8_aes_key;

    case XLNX_EFUSE_MAP_USER0_KEY:
        *len = ARRAY_SIZE(xc2vp3202_efuse_u8_user0_key);
        return xc2vp3202_efuse_u8_user0_key;

    case XLNX_EFUSE_MAP_USER1_KEY:
        *len = ARRAY_SIZE(xc2vp3202_efuse_u8_user1_key);
        return xc2vp3202_efuse_u8_user1_key;

    case XLNX_EFUSE_MAP_UDS:
        *len = ARRAY_SIZE(xc2vp3202_efuse_u8_uds);
        return xc2vp3202_efuse_u8_uds;

    case XLNX_EFUSE_MAP_SYSMON:
        *len = ARRAY_SIZE(pmx_efuse_u8_sysmon_rd64);
        return pmx_efuse_u8_sysmon_rd64;

    case XLNX_EFUSE_MAP_PUF:
        *len = ARRAY_SIZE(xc2vp3202_efuse_puf);
        return xc2vp3202_efuse_puf;

    default:
        return NULL;
    }
}

static const uint8_t *xc2vp3202_efuse_mapping_get_ac(XlnxEfuseMapIf *iface,
                                                   XlnxEfuseAccessCtrlIdx idx,
                                                   size_t *len)
{
    switch (idx) {
    case XLNX_EFUSE_AC_RD_ONLY:
        *len = ARRAY_SIZE(xc2vp3202_efuse_ac_rd_only);
        return xc2vp3202_efuse_ac_rd_only;

    case XLNX_EFUSE_AC_WR_ONLY:
        *len = ARRAY_SIZE(xc2vp3202_efuse_ac_wr_only);
        return xc2vp3202_efuse_ac_wr_only;

    default:
        return NULL;
    }

}

static size_t xc2vp3202_efuse_mapping_get_bit_idx(XlnxEfuseMapIf *iface,
                                                XlnxEfuseBitIdx bit)
{
    switch (bit) {
    case XLNX_EFUSE_BIT_GLITCH_DET_WR_LK:
        return 0x37f;

    case XLNX_EFUSE_BIT_AES_DIS:
        return 0x588;

    case XLNX_EFUSE_BIT_UDS_WR_LK:
        return 0x58c;

    case XLNX_EFUSE_BIT_PPK0_WR_LK:
        return 0x58e;

    case XLNX_EFUSE_BIT_PPK1_WR_LK:
        return 0x58f;

    case XLNX_EFUSE_BIT_PPK2_WR_LK:
        return 0x5a8;

    case XLNX_EFUSE_BIT_PPK3_WR_LK:
        return 0x3f8;

    case XLNX_EFUSE_BIT_PPK4_WR_LK:
        return 0x3f9;

    case XLNX_EFUSE_BIT_PPK5_WR_LK:
        return 0x3fa;

    case XLNX_EFUSE_BIT_PPK6_WR_LK:
        return 0x3fb;

    case XLNX_EFUSE_BIT_PPK7_WR_LK:
        return 0x3fc;

    case XLNX_EFUSE_BIT_PPK8_WR_LK:
        return 0x3fd;

    case XLNX_EFUSE_BIT_AES_CRC_LK_0:
        return 0x5a9;

    case XLNX_EFUSE_BIT_AES_CRC_LK_1:
        return 0x5aa;

    case XLNX_EFUSE_BIT_AES_WR_LK:
        return 0x5ab;

    case XLNX_EFUSE_BIT_USER_KEY_0_CRC_LK_0:
        return 0x5ac;

    case XLNX_EFUSE_BIT_USER_KEY_0_WR_LK:
        return 0x5ad;

    case XLNX_EFUSE_BIT_USER_KEY_1_CRC_LK_0:
        return 0x5ae;

    case XLNX_EFUSE_BIT_USER_KEY_1_WR_LK:
        return 0x5af;

    case XLNX_EFUSE_BIT_PUF_SYN_LK:
        return 0x5c8;

    case XLNX_EFUSE_BIT_PUF_DIS:
        return 0x5ca;

    case XLNX_EFUSE_BIT_BOOT_ENV_WR_LK:
        return 0x5ec;

    case XLNX_EFUSE_BIT_SVD_WR_LK:
        return 0x5ea;

    case XLNX_EFUSE_BIT_DNA_WR_LK:
        return 0x5eb;

    case XLNX_EFUSE_BIT_CAHER_WR_LK:
        return 0x5ed;

    case XLNX_EFUSE_BIT_GLITCH_DET_EN:
        return 0x2fd;

    default:
        return 0;
    }
}

static void xc2vp3202_efuse_mapping_class_init(ObjectClass *c, const void *data)
{
    XlnxEfuseMapIfClass *xpefmic = XLNX_EFUSE_MAP_IF_CLASS(c);

    xpefmic->get_mapping = xc2vp3202_efuse_mapping_get;
    xpefmic->get_ac_mapping = xc2vp3202_efuse_mapping_get_ac;
    xpefmic->get_bit_idx = xc2vp3202_efuse_mapping_get_bit_idx;
}

static const TypeInfo xc2vp3202_efuse_mapping_info = {
    .name = TYPE_XLNX_EFUSE_MAP_XC2VP3202,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(XlnxEfuseMapXc2vp3202State),
    .class_init = xc2vp3202_efuse_mapping_class_init,
    .interfaces = (InterfaceInfo []) {
        { TYPE_XLNX_EFUSE_MAP_IF },
        { }
    },
};

static void xc2vp3202_efuse_mapping_register_types(void)
{
    type_register_static(&xc2vp3202_efuse_mapping_info);
}

type_init(xc2vp3202_efuse_mapping_register_types)
