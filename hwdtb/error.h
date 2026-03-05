/*
 * HWDTB parsing/contextual errors
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HWDTB_ERROR_H
#define HWDTB_ERROR_H

#include "qemu/log.h"
#include "qemu/hwdtb.h"

#define HWDTB_ERR_PHANDLE_NOT_FOUND \
    "%s: cannot find node with phandle %" PRIu32

#define HWDTB_ERR_REG_CELLS_TOO_BIG_UNIMP \
    "unsupported `%s' property value > 2. " \
    "Corresponding reg values will be truncated to 64 bits"

#define HWDTB_ERR_UNEXPECTED_END_OF_PROP \
    "unexpected end of property %s"

#define HWDTB_ERR_SPEC_NOT_ENOUGH_CELLS \
    "not enough cells in the `%s' property " \
    "(parent's `%s' is %" PRIu32 ")"

#define HWDTB_ERR_NO_PARENT \
    "no parent specified for the `%s' property"

#define HWDTB_ERR_MISSING_PROP \
    "missing `%s' property"

#define HWDTB_ERR_MISSING_PROP_OR_PROP \
    "missing `%s' or `%s' property"

#define HWDTB_ERR_DEFAULT_VAL_U64 \
    "defaulting to %" PRIu64

#define HWDTB_ERR_INVAL_PROP \
    "cannot parse `%s' property"

#define HWDTB_ERR_MISSING_OR_INVAL_PROP \
    "missing or invalid `%s' property"

#define HWDTB_ERR_DUPLICATE_MAP_ENTRY \
    "ignoring duplicate %s[%s] entry"

#define HWDTB_ERR_TYPE_MISMATCH \
    "is not a `%s'"

#define HWDTB_ERR_TYPE_MISMATCH_2 \
    HWDTB_ERR_TYPE_MISMATCH " or a %s"

#define HWDTB_ERR_CLOCK_INPUT_NOT_FOUND \
    "clock input `%s' not found on device"

#define HWDTB_ERR_CLOCK_OUTPUT_NOT_FOUND \
    "clock output `%s' not found on device"

#define HWDTB_ERR_GPIO_GET_INPUT \
    "cannot get [%s] as an input GPIO: "

#define HWDTB_ERR_GPIO_CONNECT_OUTPUT \
    "cannot connect [%s,%zu] as an output GPIO: "

#define HWDTB_ERR_GPIO_REASON_RES_FAILURE \
    "resolution failure"

#define HWDTB_ERR_GPIO_REASON_MAP_EXPANSION_FAILED \
    "map property expansion failed"

/* Connection ID string prefix */
#define HWDTB_ERR_CONN \
    "%s: "

#define HWDTB_ERR_PARENT "parent node %s"
#define HWDTB_ERR_ON_PARENT "on parent node %s"

#define HWDTB_ERR2(err_, suff_) \
    glue(HWDTB_ERR_, err_) " " glue(HWDTB_ERR_, suff_)

#define HWDTB_ERR3(pref_, err_, suff_) \
    glue(HWDTB_ERR_, pref_) " " HWDTB_ERR2(err_, suff_)

#define hwdtb_report_err(node_, tpl_, ...) \
    qemu_log_mask(LOG_FDT, "%s: " tpl_ "\n", (node_)->path, ## __VA_ARGS__)

#endif
