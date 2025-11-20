/*
 * Microblaze-V CPU time source
 *
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_RISCV_MICROBLAZE_V_TIMESRC_H
#define HW_RISCV_MICROBLAZE_V_TIMESRC_H

#include "target/riscv/cpu-qom.h"
#include "hw/qdev-core.h"

#define TYPE_MICROBLAZE_V_TIMESRC "microblaze-v-timesrc"
OBJECT_DECLARE_SIMPLE_TYPE(MicroblazeVTimesrc, MICROBLAZE_V_TIMESRC)

struct MicroblazeVTimesrc {
    DeviceState parent_obj;

    uint32_t frequency;
};

#endif
