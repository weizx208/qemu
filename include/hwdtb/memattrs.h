/*
 * HWDTB MemTxAttrs QOM binding
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HWDTB_MEMATTRS_H
#define HWDTB_MEMATTRS_H

#include "qom/object.h"
#include "exec/memattrs.h"

#define TYPE_HWDTB_MEMTXATTRS "hwdtb-memattrs"
OBJECT_DECLARE_SIMPLE_TYPE(HwDtbMemTxAttrs, HWDTB_MEMTXATTRS)

struct HwDtbMemTxAttrs {
    Object parent;

    MemTxAttrs attrs;
};

static inline MemTxAttrs hwdtb_memattrs_get(const HwDtbMemTxAttrs *s)
{
    if (s == NULL) {
        return MEMTXATTRS_UNSPECIFIED;
    }

    return s->attrs;
}

#endif
