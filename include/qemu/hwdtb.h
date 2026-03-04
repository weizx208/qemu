/*
 * HWDTB sub-system
 *
 * Construct a QEMU machine from a "hwdtb" description.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_HWDTB_H
#define QEMU_HWDTB_H

#include "qemu/queue.h"
#include "qom/object.h"
#include <libfdt.h>

typedef struct HwDtb HwDtb;

/**
 * The main HwDtb structure
 *
 * @machine the machine being constructed
 * @fdt the hwdtb being parsed
 */
struct HwDtb {
    MachineState *machine;
    void *fdt;
};

/**
 * hwdtb_create_machine
 *
 * Main hwdtb entry point. Create the machine described by @fdt.
 *
 * @machine the machine to populate
 * @fdt the flattened device tree representing the hwdtb machine
 *
 * @return the corresponding HwDtb structure.
 */
HwDtb *hwdtb_create_machine(MachineState *machine, void *fdt);

/**
 * hwdtb_create_machine_oneshot
 *
 * Same as hwdtb_create_machine but does not return the HwDtb structure.
 *
 * @machine the machine to populate
 * @fdt the flattened device tree representing the hwdtb machine
 */
void hwdtb_create_machine_oneshot(MachineState *machine, void *fdt);

/**
 * hwdtb_free
 *
 * Free a hwdtb created with hwdtb_create_machine. Can be called safely after
 * machine creation. The machine in itself is untouched.
 */
void hwdtb_free(HwDtb *hwdtb);

#endif
