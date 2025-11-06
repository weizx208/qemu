/*
 * QEMU RISC-V CPU QOM header (target agnostic)
 *
 * Copyright (c) 2023 Ventana Micro Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RISCV_CPU_QOM_H
#define RISCV_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_RISCV_CPU "riscv-cpu"
#define TYPE_RISCV_DYNAMIC_CPU "riscv-dynamic-cpu"

#define RISCV_CPU_TYPE_SUFFIX "-" TYPE_RISCV_CPU
#define RISCV_CPU_TYPE_NAME(name) (name RISCV_CPU_TYPE_SUFFIX)

#define TYPE_RISCV_CPU_ANY              RISCV_CPU_TYPE_NAME("any")
#define TYPE_RISCV_CPU_MAX              RISCV_CPU_TYPE_NAME("max")
#define TYPE_RISCV_CPU_BASE32           RISCV_CPU_TYPE_NAME("rv32")
#define TYPE_RISCV_CPU_BASE64           RISCV_CPU_TYPE_NAME("rv64")
#define TYPE_RISCV_CPU_BASE128          RISCV_CPU_TYPE_NAME("x-rv128")
#define TYPE_RISCV_CPU_IBEX             RISCV_CPU_TYPE_NAME("lowrisc-ibex")
#define TYPE_RISCV_CPU_SHAKTI_C         RISCV_CPU_TYPE_NAME("shakti-c")
#define TYPE_RISCV_CPU_SIFIVE_E31       RISCV_CPU_TYPE_NAME("sifive-e31")
#define TYPE_RISCV_CPU_SIFIVE_E34       RISCV_CPU_TYPE_NAME("sifive-e34")
#define TYPE_RISCV_CPU_SIFIVE_E51       RISCV_CPU_TYPE_NAME("sifive-e51")
#define TYPE_RISCV_CPU_SIFIVE_U34       RISCV_CPU_TYPE_NAME("sifive-u34")
#define TYPE_RISCV_CPU_SIFIVE_U54       RISCV_CPU_TYPE_NAME("sifive-u54")
#define TYPE_RISCV_CPU_THEAD_C906       RISCV_CPU_TYPE_NAME("thead-c906")
#define TYPE_RISCV_CPU_VEYRON_V1        RISCV_CPU_TYPE_NAME("veyron-v1")
#define TYPE_RISCV_CPU_MICROBLAZE_V     RISCV_CPU_TYPE_NAME("microblaze-v")
#define TYPE_RISCV_CPU_HOST             RISCV_CPU_TYPE_NAME("host")

OBJECT_DECLARE_CPU_TYPE(RISCVCPU, RISCVCPUClass, RISCV_CPU)

#define TYPE_RISCV_CPU_TIME_SRC_IF "riscv-cpu-time-src-if"

typedef struct RISCVCPUTimeSrcIfClass RISCVCPUTimeSrcIfClass;
DECLARE_CLASS_CHECKERS(RISCVCPUTimeSrcIfClass, RISCV_CPU_TIME_SRC_IF,
                       TYPE_RISCV_CPU_TIME_SRC_IF)
#define RISCV_CPU_TIME_SRC_IF(obj) \
        INTERFACE_CHECK(RISCVCPUTimeSrcIf, (obj), TYPE_RISCV_CPU_TIME_SRC_IF)

typedef struct RISCVCPUTimeSrcIf RISCVCPUTimeSrcIf;

/**
 * RISCVCPUTimeSrcIf interface
 *
 * This interface is used by CPUs implementing the sstc extension. When the CPU
 * implements this extension, it must have a time source to implement the sstc
 * timers. Devices implementing this interface provide a monotonic tick counter
 * and the associated tick frequency so that the CPU code can compute timer
 * deadlines.
 */
struct RISCVCPUTimeSrcIfClass {
    InterfaceClass parent_class;

    /**
     * get_ticks: get the current value of the free running counter associated
     * with this time source.
     */
    uint64_t (*get_ticks)(RISCVCPUTimeSrcIf *);

    /**
     * get_tick_freq: get the tick frequency of this time source.
     */
    uint32_t (*get_tick_freq)(RISCVCPUTimeSrcIf *);
};

#endif /* RISCV_CPU_QOM_H */
