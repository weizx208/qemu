/*
 * HWDTB utility functions
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/hwdtb.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "hw/qdev-core.h"
#include "system/memory.h"

#include <libfdt.h>

HwDtb *hwdtb_create_machine(MachineState *machine, void *fdt)
{
    HwDtb *hwdtb;

    hwdtb = g_new0(HwDtb, 1);
    hwdtb->machine = machine;
    hwdtb->fdt = fdt;

    memory_region_transaction_begin();

    /* TODO */

    memory_region_transaction_commit();

    return hwdtb;
}

void hwdtb_create_machine_oneshot(MachineState *machine, void *fdt)
{
    hwdtb_free(hwdtb_create_machine(machine, fdt));
}

void hwdtb_free(HwDtb *hwdtb)
{
    g_free(hwdtb);
}
