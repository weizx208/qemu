/*
 * HWDTB MemTxAttrs QOM binding
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/visitor.h"
#include "hwdtb/memattrs.h"

static bool memattr_get_secure(Object *obj, Error **errp)
{
    HwDtbMemTxAttrs *s = HWDTB_MEMTXATTRS(obj);

    return s->attrs.secure;
}

static void memattr_set_secure(Object *obj, bool value, Error **errp)
{
    HwDtbMemTxAttrs *s = HWDTB_MEMTXATTRS(obj);

    s->attrs.unspecified = false;
    s->attrs.secure = value;
}
static void memattr_get_requester_id(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    HwDtbMemTxAttrs *s = HWDTB_MEMTXATTRS(obj);
    uint16_t value;

    if (visit_type_uint16(v, name, &value, errp)) {
        s->attrs.requester_id = value;
    }
}

static void memattr_set_requester_id(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    HwDtbMemTxAttrs *s = HWDTB_MEMTXATTRS(obj);
    uint16_t value;

    if (!visit_type_uint16(v, name, &value, errp)) {
        return;
    }

    s->attrs.unspecified = false;
    s->attrs.requester_id = value;
}

static void hwdtb_mem_tx_attrs_init(Object *obj)
{
    HwDtbMemTxAttrs *s = HWDTB_MEMTXATTRS(obj);

    s->attrs = MEMTXATTRS_UNSPECIFIED;
}

static void hwdtb_mem_tx_attrs_class_init(ObjectClass *oc, const void *data)
{
    object_class_property_add_bool(oc, "secure", memattr_get_secure,
                                   memattr_set_secure);

    object_class_property_add(oc, "requester-id", "uint16",
                              memattr_get_requester_id,
                              memattr_set_requester_id, NULL, NULL);
}

static const TypeInfo hwdtb_mem_tx_attrs_info = {
    .parent = TYPE_OBJECT,
    .name = TYPE_HWDTB_MEMTXATTRS,
    .instance_size = sizeof(HwDtbMemTxAttrs),
    .class_init = hwdtb_mem_tx_attrs_class_init,
    .instance_init = hwdtb_mem_tx_attrs_init,
};

static void hwdtb_mem_tx_attrs_register_types(void)
{
    type_register_static(&hwdtb_mem_tx_attrs_info);
}

type_init(hwdtb_mem_tx_attrs_register_types);

