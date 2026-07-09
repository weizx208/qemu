/*
 * HWDTB proxy object
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/hwdtb.h"

Object *hwdtb_create_proxy(Object *obj, HwDtbProxyFlags flags)
{
    Object *ret = object_new(TYPE_HWDTB_PROXY);
    HwDtbProxyState *proxy = HWDTB_PROXY(ret);

    g_assert(obj->parent);
    object_property_set_link(ret, "proxy", obj, &error_abort);
    proxy->flags = flags;

    return ret;
}

bool hwdtb_is_proxy(const HwDtbNode *node)
{
    return object_dynamic_cast(node->obj, TYPE_HWDTB_PROXY) != NULL;
}

bool hwdtb_proxy_has_flags(const HwDtbNode *node, HwDtbProxyFlags flags)
{
    if (!hwdtb_is_proxy(node)) {
        return false;
    }

    return (HWDTB_PROXY(node->obj)->flags & flags) == flags;
}

bool hwdtb_is_proxy_to_local(const HwDtbNode *node)
{
    return hwdtb_proxy_has_flags(node, HWDTB_PROXY_LOCAL_OBJ);
}

bool hwdtb_is_proxy_to_foreign(const HwDtbNode *node)
{
    if (!hwdtb_is_proxy(node)) {
        return false;
    }

    return !hwdtb_is_proxy_to_local(node);
}

static void hwdtb_proxy_class_init(ObjectClass *oc, const void *data)
{
    object_class_property_add_link(oc, "proxy", TYPE_OBJECT,
                                   offsetof(HwDtbProxyState, proxy),
                                   object_property_allow_set_link, 0);
}

static const TypeInfo hwdtb_proxy_info = {
    .parent = TYPE_OBJECT,
    .name = TYPE_HWDTB_PROXY,
    .instance_size = sizeof(HwDtbProxyState),
    .class_init = hwdtb_proxy_class_init,
};

static void hwdtb_proxy_register_types(void)
{
    type_register_static(&hwdtb_proxy_info);
}

type_init(hwdtb_proxy_register_types);
