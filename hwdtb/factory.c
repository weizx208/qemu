/*
 * HWDTB node factories
 *
 * Those functions are used during the creation phase. They return an instance
 * of a QOM object. The factory function selection for a given node is the
 * result of the resolve pass.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qom/object.h"
#include "error.h"
#include "trace.h"

typedef struct CompatTranslate {
    const char *from;
    const char *to;
} CompatTranslate;

typedef struct CompatHandler {
    const char *compat;
    HwDtbObjectFactory factory;
} CompatHandler;

static Object *hwdtb_factory_from_oc(HwDtbNode *node)
{
    return object_new_with_class(node->oc);
}

static const CompatTranslate STATIC_TRANSLATE_TABLE[] = {
};

static const CompatHandler STATIC_COMPAT_HANDLER[] = {
};

const char *hwdtb_compat_translate(const char *compat)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(STATIC_TRANSLATE_TABLE); i++) {
        if (!strcmp(compat, STATIC_TRANSLATE_TABLE[i].from)) {
            return STATIC_TRANSLATE_TABLE[i].to;
        }
    }

    return NULL;
}

HwDtbObjectFactory hwdtb_get_factory_for_compat(const char *compat)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(STATIC_COMPAT_HANDLER); i++) {
        if (!strcmp(compat, STATIC_COMPAT_HANDLER[i].compat)) {
            return STATIC_COMPAT_HANDLER[i].factory;
        }
    }

    return NULL;
}

HwDtbObjectFactory hwdtb_get_default_factory(void)
{
    return hwdtb_factory_from_oc;
}
