/*
 * fault_injection.c
 *
 * Copyright (C) 2016 Fred KONRAD <fred.konrad@greensocs.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/help-texts.h"
#include "cpu.h"
#include "qapi/qmp/qerror.h"
#include "qobject/qjson.h"
#include "qapi/qapi-commands-ui.h"
#include "qapi/qapi-events-ui.h"
#include "qapi/qapi-events-injection.h"
#include "qapi/error.h"
#include "qapi/qapi-types-injection.h"
#include "qapi/qapi-commands-injection.h"
#include "qemu/timer.h"
#include "system/memory.h"
#include "hw/irq.h"
#include "qemu/log.h"

#ifndef DEBUG_FAULT_INJECTION
#define DEBUG_FAULT_INJECTION 0
#endif

#define DPRINTF(fmt, ...) do {                                                 \
    if (DEBUG_FAULT_INJECTION) {                                               \
        qemu_log("fault_injection: " fmt , ## __VA_ARGS__);                    \
    }                                                                          \
} while (0)

void qmp_write_mem(int64_t addr, int64_t val, int64_t size, bool has_cpu,
                   int64_t cpu, const char *qom, Error **errp)
{
    int cpu_id = 0;
    Object *obj;
    CPUState *s;

    if (qom) {
        obj = object_resolve_path(qom, NULL);
        s = (CPUState *)object_dynamic_cast(obj, TYPE_CPU);
        if (s) {
            cpu_id = s->cpu_index;
            DPRINTF("write memory addr=0x%" PRIx64 " val=0x%" PRIx64
                    " ld size=%"PRId64" cpu_path=%s (cpu=%d)\n", addr, val,
                    size, qom, cpu_id);
        } else {
            error_setg(errp, "'%s' is not a CPU or doesn't exists", qom);
            DPRINTF("write memory failed.\n");
            return;
        }
    } else {
        if (has_cpu) {
            cpu_id = cpu;
        }
        DPRINTF("write memory addr=0x%" PRIx64 "val=0x%" PRIx64 " "
                "size=%"PRId64" cpu=%d\n", addr, val, size, cpu_id);
    }

    if (address_space_write(cpu_get_address_space(qemu_get_cpu(cpu_id), 0),
                            addr, MEMTXATTRS_UNSPECIFIED, ((uint8_t *)&val), size)) {
        DPRINTF("write memory failed.\n");
    } else {
        DPRINTF("write memory succeed.\n");
    }
}

ReadValue *qmp_read_mem(int64_t addr, int64_t size, bool has_cpu, int64_t cpu,
                        const char *qom, Error **errp)
{
    ReadValue *ret = g_new0(ReadValue, 1);
    int cpu_id = 0;
    Object *obj;
    CPUState *s;

    ret->value = 0;

    if (qom) {
        obj = object_resolve_path(qom, NULL);
        s = (CPUState *)object_dynamic_cast(obj, TYPE_CPU);
        if (s) {
            cpu_id = s->cpu_index;
            DPRINTF("read memory addr=0x%" PRIx64 " size=%"PRId64" cpu_path=%s"
                    " (cpu=%d)\n", addr, size, qom, cpu_id);
        } else {
            error_setg(errp, "'%s' is not a CPU or doesn't exists", qom);
            DPRINTF("read memory failed.\n");
            return ret;
        }
    } else {
        if (has_cpu) {
            cpu_id = cpu;
        }
        DPRINTF("read memory addr=0x%" PRIx64 " size=%"PRId64" (cpu=%d)\n",
                addr, size, cpu_id);
    }

    if (address_space_read(cpu_get_address_space(qemu_get_cpu(cpu_id), 0), addr,
                           MEMTXATTRS_UNSPECIFIED, (uint8_t *) &(ret->value), size)) {
        DPRINTF("read memory failed.\n");
        return ret;
    } else {
        DPRINTF("read memory succeed 0x%" PRIx64 ".\n", ret->value);
        return ret;
    }
}

