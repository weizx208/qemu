/*
 * AMD DDR Memory Device
 *
 * Copyright (c) Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/misc/amd-ddr-memory.h"

static void amd_ddr_memory_realize(DeviceState *dev, Error **errp)
{
    AMDDDRMemory *s = AMD_DDR_MEMORY(dev);
    g_autofree char *name = NULL;

    if (s->size == 0) {
        error_setg(errp, "ddr-memory: size must be non-zero");
        return;
    }

    if (s->max_ram_property < 1 && s->max_ram_property > 2) {
        error_setg(errp, "ddr-memory: max-ram-property must be 1 or 2");
        return;
    }

    name = g_strdup_printf("ddr@0x%" PRIx64, (uint64_t)s->address);
    memory_region_init_ram(&s->mr, OBJECT(dev), name, s->size, errp);
}

static Property amd_ddr_memory_props[] = {
    DEFINE_PROP_UINT64("address", AMDDDRMemory, address, 0),
    DEFINE_PROP_UINT64("size", AMDDDRMemory, size, 0),
    DEFINE_PROP_UINT8("max-ram-property", AMDDDRMemory, max_ram_property, 2),
    DEFINE_PROP_END_OF_LIST(),
};

static void amd_ddr_memory_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = amd_ddr_memory_realize;
    device_class_set_props(dc, amd_ddr_memory_props);
}

static const TypeInfo amd_ddr_memory_info = {
    .parent             = TYPE_DEVICE,
    .name               = TYPE_AMD_DDR_MEMORY,
    .instance_size      = sizeof(AMDDDRMemory),
    .class_init         = amd_ddr_memory_class_init,
};

static void ddr_register_types(void)
{
    type_register_static(&amd_ddr_memory_info);
}

type_init(ddr_register_types)
