/*
 * AMD DDR Memory Device - Header file
 *
 * Copyright (c) Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_AMD_DDR_MEMORY_H
#define HW_MISC_AMD_DDR_MEMORY_H

#include "hw/qdev-core.h"
#include "exec/hwaddr.h"
#include "exec/memory.h"

#define TYPE_AMD_DDR_MEMORY "amd-ddr-memory"

OBJECT_DECLARE_SIMPLE_TYPE(AMDDDRMemory, AMD_DDR_MEMORY)

struct AMDDDRMemory {
    DeviceState parent_obj;
    MemoryRegion mr;
    hwaddr address;
    uint64_t size;
    uint8_t max_ram_property;
};

#endif /* HW_MISC_AMD_DDR_MEMORY_H */
