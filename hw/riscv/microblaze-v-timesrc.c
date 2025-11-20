/*
 * Microblaze-V CPU time source
 *
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "hw/qdev-properties.h"
#include "hw/riscv/microblaze-v-timesrc.h"

static uint64_t microblaze_v_time_src_get_ticks(RISCVCPUTimeSrcIf *iface)
{
    uint32_t f = MICROBLAZE_V_TIMESRC(iface)->frequency;

    return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), f,
                    NANOSECONDS_PER_SECOND);
}

static uint32_t microblaze_v_time_src_get_tick_freq(RISCVCPUTimeSrcIf *iface)
{
    return MICROBLAZE_V_TIMESRC(iface)->frequency;
}

static Property microblaze_v_timesrc_properties[] = {
    DEFINE_PROP_UINT32("frequency", MicroblazeVTimesrc, frequency,
                       NANOSECONDS_PER_SECOND /* 1 GHz */),
    DEFINE_PROP_END_OF_LIST()
};

static void microblaze_v_timesrc_class_init(ObjectClass *oc, void *data)
{
    RISCVCPUTimeSrcIfClass *rctsc = RISCV_CPU_TIME_SRC_IF_CLASS(oc);

    rctsc->get_ticks = microblaze_v_time_src_get_ticks;
    rctsc->get_tick_freq = microblaze_v_time_src_get_tick_freq;
    device_class_set_props(DEVICE_CLASS(oc), microblaze_v_timesrc_properties);
}

static const TypeInfo microblaze_v_timesrc_info = {
    .name          = TYPE_MICROBLAZE_V_TIMESRC,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(MicroblazeVTimesrc),
    .class_init    = microblaze_v_timesrc_class_init,
    .interfaces    = (InterfaceInfo []) {
        { TYPE_RISCV_CPU_TIME_SRC_IF },
        { }
    }
};

static void microblaze_v_timesrc_register_types(void)
{
    type_register_static(&microblaze_v_timesrc_info);
}

type_init(microblaze_v_timesrc_register_types)
