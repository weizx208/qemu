/*
 * HWDTB parser
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "trace.h"

#include <libfdt.h>

typedef struct HwDtbParseCtx {
    GString *path;
} HwDtbParseCtx;

static HwDtbNode *hwdtb_node_new(HwDtb *hwdtb, int offset, const GString *path)
{
    HwDtbNode *ret;

    ret = g_new0(HwDtbNode, 1);
    ret->offset = offset;
    ret->hwdtb = hwdtb;

    if (!*path->str) {
        ret->path = g_strdup("/");
    } else {
        ret->path = g_strdup(path->str);
    }

    QSIMPLEQ_INIT(&ret->children);

    return ret;
}

static void add_node_to_hash_tables(HwDtbNode *node)
{
    uint32_t phandle;

    if (hwdtb_node_get_prop_uint32(node, "phandle", &phandle)) {
        trace_hwdtb_node_phandle(node->path, phandle);
        g_hash_table_insert(node->hwdtb->node_by_phandle,
                            GUINT_TO_POINTER(phandle), node);
    }
    g_hash_table_insert(node->hwdtb->node_by_path, (gpointer)node->path, node);
}

static HwDtbNode *hwdtb_parse_node(HwDtb *hwdtb, int offset, HwDtbParseCtx *ctx)
{
    HwDtbNode *node;
    const char *name;
    int subnode;

    name = fdt_get_name(hwdtb->fdt, offset, NULL);
    g_string_append_printf(ctx->path, "%s", name);

    node = hwdtb_node_new(hwdtb, offset, ctx->path);
    trace_hwdtb_node_parse(node->path);

    g_string_append_c(ctx->path, '/');

    fdt_for_each_subnode(subnode, hwdtb->fdt, offset) {
        HwDtbNode *child = hwdtb_parse_node(hwdtb, subnode, ctx);

        child->parent = node;
        QSIMPLEQ_INSERT_TAIL(&node->children, child, link);
    }

    g_string_truncate(ctx->path, ctx->path->len - (strlen(name) + 1));

    add_node_to_hash_tables(node);

    return node;
}

void hwdtb_parse(HwDtb *hwdtb)
{
    int root_offset = fdt_next_node(hwdtb->fdt, -1, NULL);
    HwDtbParseCtx ctx = {
        .path = g_string_new(""),
    };

    hwdtb->root = hwdtb_parse_node(hwdtb, root_offset, &ctx);

    g_string_free(ctx.path, true);
}
