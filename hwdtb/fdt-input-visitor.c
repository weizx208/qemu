/*
 * FDT property input visitor
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "qapi/error.h"
#include "qapi/visitor-impl.h"
#include "qapi/qmp/qerror.h"
#include "qemu/hwdtb.h"
#include "qemu/cutils.h"
#include "trace.h"

#include <libfdt.h>

typedef struct FDTInputVisitor {
    Visitor visitor;
    HwDtbNode *node;

    struct {
        const struct fdt_property *prop;
        size_t offset;
    } cur_list;

} FDTInputVisitor;

#define HWDTB_FDT_PARSE_ERR "Cannot parse %s/%s as %s"

static bool visit_fdt_list_get_next_u32(FDTInputVisitor *v, uint32_t *cell)
{
    fdt32_t *data;
    fdt32_t len;
    const struct fdt_property *prop = v->cur_list.prop;

    len = fdt32_ld(&prop->len);

    if ((len - v->cur_list.offset) < sizeof(fdt32_t)) {
        return false;
    }

    data = (fdt32_t *) (v->cur_list.prop->data + v->cur_list.offset);
    *cell = fdt32_ld(data);
    v->cur_list.offset += sizeof(*data);

    return true;
}

static const char *visit_fdt_list_get_string(FDTInputVisitor *v)
{
    fdt32_t len;
    const struct fdt_property *prop = v->cur_list.prop;
    const char *ret, *next;

    len = fdt32_ld(&prop->len);

    if (len == v->cur_list.offset) {
        return NULL;
    }

    ret = prop->data + v->cur_list.offset;

    next = hwdtb_fdt_prop_parse_next_string(prop, ret);
    if (next == NULL) {
        v->cur_list.offset = len;
    } else {
        v->cur_list.offset = next - prop->data;
    }

    return ret;
}

static bool visit_fdt_get_next_variable_size(FDTInputVisitor *v,
                                             const char *name, uint64_t *cell)
{
    if (v->cur_list.prop) {
        uint32_t u32;
        bool ret;

        ret = visit_fdt_list_get_next_u32(v, &u32);
        *cell = u32;
        return ret;
    } else {
        return hwdtb_node_get_prop_uint(v->node, name, cell);
    }
}

static bool visit_fdt_get_next_u32(FDTInputVisitor *v,
                                   const char *name, uint32_t *cell)
{
    if (v->cur_list.prop) {
        return visit_fdt_list_get_next_u32(v, cell);
    } else {
        return hwdtb_node_get_prop_uint32(v->node, name, cell);
    }
}

static const char * visit_fdt_get_next_string(FDTInputVisitor *v,
                                              const char *name)
{
    if (v->cur_list.prop) {
        return visit_fdt_list_get_string(v);
    } else {
        return hwdtb_node_get_prop_string(v->node, name);
    }
}

static bool visit_fdt_prop_uint64(Visitor *base_v, const char *name,
                                  uint64_t *obj, Error **errp)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;

    if (!visit_fdt_get_next_variable_size(v, name, obj)) {
        error_setg(errp, HWDTB_FDT_PARSE_ERR,
                   v->node->path, name, "uint");
        return false;
    }

    trace_hwdtb_node_set_prop_uint(v->node->path, name, *obj);
    return true;
}

static bool visit_fdt_prop_int64(Visitor *base_v, const char *name,
                                 int64_t *obj, Error **errp)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;
    uint64_t val;

    if (!visit_fdt_get_next_variable_size(v, name, &val)) {
        error_setg(errp, HWDTB_FDT_PARSE_ERR,
                   v->node->path, name, "int");
        return false;
    }

    *obj = (int64_t) val;
    trace_hwdtb_node_set_prop_int(v->node->path, name, *obj);
    return true;
}

static bool visit_fdt_prop_number(Visitor *base_v, const char *name,
                                  double *obj, Error **errp)
{
    uint64_t val;
    bool ret;

    /* fdt has no floating point representation. Always parse as uint */
    ret = visit_fdt_prop_uint64(base_v, name, &val, errp);

    if (ret) {
        *obj = val;
    }

    return ret;
}

static bool visit_fdt_prop_bool(Visitor *base_v, const char *name,
                                bool *obj, Error **errp)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;
    uint64_t val;

    if (!visit_fdt_get_next_variable_size(v, name, &val)) {
        error_setg(errp, HWDTB_FDT_PARSE_ERR,
                   v->node->path, name, "bool");
        return false;
    }

    *obj = val;
    trace_hwdtb_node_set_prop_bool(v->node->path, name, *obj);
    return true;
}

static bool visit_fdt_prop_link(FDTInputVisitor *v, const char *name,
                                char **obj, Error **errp)
{
    uint32_t phandle;
    HwDtbNode *link;
    Object *link_obj;
    g_autofree char *target_prop = NULL;

    /*
     * We expect to find a phandle. We must return the corresponding object
     * canonical path.
     */
    if (!visit_fdt_get_next_u32(v, name, &phandle)) {
        error_setg(errp, "Cannot parse %s/%s as uint32\n", v->node->path, name);
        return false;
    }

    link = hwdtb_get_node_by_phandle(v->node->hwdtb, phandle);

    if (link == NULL) {
        error_setg(errp, "Cannot find node with phandle %" PRIu32 "\n",
                   phandle);
        return false;
    }

    /*
     * -- Legacy --
     * The legacy fdt_generic code has this hack where it looks for a
     * <propname>-target link property on the linked object. If found, it uses
     * the value of this link property as the linked object instead of the one
     * specified in the hwdtb.
     */
    target_prop = g_strconcat(name, "-target", NULL);
    link_obj = hwdtb_get_obj(link);

    if (object_property_find(link_obj, target_prop)) {
        Object *proxy = object_property_get_link(link_obj, target_prop, errp);

        if (proxy) {
            *obj = object_get_canonical_path(proxy);

            trace_hwdtb_node_set_prop_link_proxy(v->node->path, name, *obj);
            return true;
        }

    }

    *obj = object_get_canonical_path(link_obj);
    trace_hwdtb_node_set_prop_link(v->node->path, name, *obj);
    return true;
}

static bool visit_fdt_prop_str(Visitor *base_v, const char *name, char **obj,
                               Error **errp)
{
    ObjectProperty *obj_prop;
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;
    const struct fdt_property *prop;
    const char *str;

    *obj = NULL;
    prop = hwdtb_node_get_prop(v->node, name, NULL);

    if (prop == NULL) {
        return false;
    }

    obj_prop = object_property_find_err(hwdtb_get_obj(v->node), name,
                                        &error_abort);

    if (strstart(obj_prop->type, "link<", NULL)) {
        return visit_fdt_prop_link(v, name, obj, errp);
    }

    str = visit_fdt_get_next_string(v, name);

    if (str == NULL) {
        error_setg(errp, "Cannot parse %s/%s as string\n", v->node->path, name);
        return false;
    }

    *obj = g_strdup(str);
    trace_hwdtb_node_set_prop_str(v->node->path, name, *obj);
    return true;
}

static bool visit_fdt_start_list(Visitor *base_v, const char *name,
                                 GenericList **list, size_t size, Error **errp)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;

    g_assert(v->cur_list.prop == NULL);

    v->cur_list.prop = hwdtb_node_get_prop(v->node, name, NULL);

    if (v->cur_list.prop == NULL) {
        *list = NULL;
        return false;
    }

    v->cur_list.offset = 0;
    *list = g_malloc0(size);

    return true;
}

static GenericList *visit_fdt_next_list(Visitor *base_v, GenericList *tail,
                                        size_t size)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;

    g_assert(v->cur_list.prop);

    if (v->cur_list.offset == fdt32_ld(&v->cur_list.prop->len)) {
        return NULL;
    }

    tail->next = g_malloc0(size);
    return tail->next;
}

static bool visit_fdt_check_list(Visitor *base_v, Error **errp)
{
    return true;
}

static void visit_fdt_end_list(Visitor *base_v, void **obj)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;

    g_assert(v->cur_list.prop);

    v->cur_list.prop = NULL;
}

static void visit_fdt_free(Visitor *base_v)
{
    FDTInputVisitor *v = (FDTInputVisitor *) base_v;

    g_free(v);
}

Visitor *hwdtb_node_input_visitor_new(HwDtbNode *node)
{
    FDTInputVisitor *v = g_new0(FDTInputVisitor, 1);

    v->node = node;

    v->visitor.type = VISITOR_INPUT;

    v->visitor.start_list = visit_fdt_start_list;
    v->visitor.next_list = visit_fdt_next_list;
    v->visitor.check_list = visit_fdt_check_list;
    v->visitor.end_list = visit_fdt_end_list;

    v->visitor.type_bool = visit_fdt_prop_bool;
    v->visitor.type_uint64 = visit_fdt_prop_uint64;
    v->visitor.type_int64 = visit_fdt_prop_int64;
    v->visitor.type_str = visit_fdt_prop_str;
    v->visitor.type_number = visit_fdt_prop_number;

    v->visitor.free = visit_fdt_free;

    return &v->visitor;
}
