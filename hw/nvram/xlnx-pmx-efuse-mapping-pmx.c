/*
 * Xilinx PMX eFuse mapping
 *
 * Copyright Advanced Micro Devices, Inc.
 * Luc Michel <luc.michel@amd.com>
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

#include "xlnx-pmx-efuse-tile.c.inc"

OBJECT_DECLARE_SIMPLE_TYPE(XlnxEfuseMapPmxState, XLNX_EFUSE_MAP_PMX)

struct XlnxEfuseMapPmxState {
    Object parent_obj;
};

/* Bits readable as 32-bit words through the pmx-efuse-cache */
static const XlnxPmxEfuseTile pmx_efuse_u32[] = {
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
static const XlnxPmxEfuseTile pmx_efuse_u8_aes_key[] = {
    EFUSE_U8_TILES_AES_KEY
};

static const XlnxPmxEfuseTile pmx_efuse_u8_user0_key[] = {
    EFUSE_U8_TILES_USER0_KEY
};

static const XlnxPmxEfuseTile pmx_efuse_u8_user1_key[] = {
    EFUSE_U8_TILES_USER1_KEY
};

static const XlnxPmxEfuseTile pmx_efuse_u8_uds[] = {
    EFUSE_U8_TILES_UDS
};

static const XlnxPmxEfuseTile pmx_efuse_puf[] = {
    EFUSE_U32_TILES_PUF
};

static const XlnxPmxEfuseTile *pmx_efuse_mapping_pmx_get(XlnxEfuseMapIf *iface,
                                                         XlnxEfuseMapIdx idx,
                                                         size_t *len)
{
    switch (idx) {
    case XLNX_EFUSE_MAP_REG_EXPOSED:
        *len = ARRAY_SIZE(pmx_efuse_u32);
        return pmx_efuse_u32;

    case XLNX_EFUSE_MAP_AES_KEY:
        *len = ARRAY_SIZE(pmx_efuse_u8_aes_key);
        return pmx_efuse_u8_aes_key;

    case XLNX_EFUSE_MAP_USER0_KEY:
        *len = ARRAY_SIZE(pmx_efuse_u8_user0_key);
        return pmx_efuse_u8_user0_key;

    case XLNX_EFUSE_MAP_USER1_KEY:
        *len = ARRAY_SIZE(pmx_efuse_u8_user1_key);
        return pmx_efuse_u8_user1_key;

    case XLNX_EFUSE_MAP_UDS:
        *len = ARRAY_SIZE(pmx_efuse_u8_uds);
        return pmx_efuse_u8_uds;

    case XLNX_EFUSE_MAP_SYSMON:
        *len = ARRAY_SIZE(pmx_efuse_u8_sysmon_rd64);
        return pmx_efuse_u8_sysmon_rd64;

    case XLNX_EFUSE_MAP_PUF:
        *len = ARRAY_SIZE(pmx_efuse_puf);
        return pmx_efuse_puf;

    default:
        return NULL;
    }
}

static void pmx_efuse_mapping_pmx_class_init(ObjectClass *c, void *data)
{
    XlnxEfuseMapIfClass *xpefmic = XLNX_EFUSE_MAP_IF_CLASS(c);

    xpefmic->get_mapping = pmx_efuse_mapping_pmx_get;
}

static const TypeInfo pmx_efuse_mapping_pmx_info = {
    .name = TYPE_XLNX_EFUSE_MAP_PMX,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(XlnxEfuseMapPmxState),
    .class_init = pmx_efuse_mapping_pmx_class_init,
    .interfaces = (InterfaceInfo []) {
        { TYPE_XLNX_EFUSE_MAP_IF },
        { }
    },
};

static void pmx_efuse_mapping_pmx_register_types(void)
{
    type_register_static(&pmx_efuse_mapping_pmx_info);
}

type_init(pmx_efuse_mapping_pmx_register_types)
