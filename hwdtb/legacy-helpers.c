/*
 * HWDTB legacy helpers
 *
 * Those functions handle specific legacy nodes that can't be handled in the
 * general flow. Once we fix existing hwdtbs and decide we can drop backward
 * compatibility, those can go away.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "hw/intc/arm_gic_common.h"
#include "hw/misc/xlnx-versal-pmc-sysmon.h"
#include "error.h"
#include "trace.h"

/*
 * -- Legacy --
 * arm,armv8-timer node handling
 *
 * This node is inherited from Linux devicetrees and does not transpose well
 * from an hardware perspective. In a Linux devicetree there is one timer node
 * for all the CPU cores.
 *
 * In QEMU/real hardware:
 *     - We have one timer instance per CPU
 *     - The timer is actually embedded into the CPU object (thus the timer IRQs
 *       are exposed by the CPU)
 *     - In heterogeneous systems we can't know for sure what CPUs this
 *       unique timer node is referring to.
 *
 * A hwdtb friendly way to describe this would simply be to connect the lines of
 * each CPU timers back to the GIC explicitly.
 *
 * The timer node looks like this:
 *
 * timer {
 *    compatible = "arm,armv8-timer";
 *    interrupt-parent = <0x01>;
 *    interrupts = <0x01 0x0d 0xff01         -- S EL1
 *                  0x01 0x0e 0xff01         -- NS EL1
 *                  0x01 0x0b 0xff01         -- VIRT
 *                  0x01 0x0a 0xff01>;       -- NS EL2
 *    clock-frequency = <0x5f5e100>;
 * };
 *
 * Interrupts are PPIs on the GIC. The 0xff00 bits in the third cell of each
 * interrupt spec was used in the legacy code to specify CPU affinity for PPIs.
 * This seems wrong for timer interrupts. Each timer IRQ should connect to its
 * own PPI and nothing else.
 *
 * The process is as follows:
 *    - Have an heuristic establishing a list of CPUs handled by this node. For
 *      this use the first-cpu-idx and num-cpu properties of the parent GIC.
 *    - For each CPU, connect its timer outputs to its corresponding PPI on the
 *      GIC.
 */
void hwdtb_legacy_armv8_timer_connect(HwDtbNode *node, void *opaque)
{
    HwDtbNode *gic;
    Object *gic_obj;
    HwDtbConnection *first_conn, *conn;
    HwDtbConnectionTarget *first_target;
    Error *err = NULL;
    uint32_t first_cpu_idx, num_cpu, num_irq;
    size_t i, has_security_ext;

    if (QSIMPLEQ_EMPTY(&node->connection[HWDTB_CON_INTERRUPT])) {
        /* no interrupt connection */
        return;
    }

    first_conn = QSIMPLEQ_FIRST(&node->connection[HWDTB_CON_INTERRUPT]);
    first_target = &g_array_index(first_conn->targets,
                                  HwDtbConnectionTarget, 0);

    hwdtb_node_foreach_connection(conn, node, HWDTB_CON_INTERRUPT) {
        HwDtbConnectionTarget *target;

        if (conn->targets->len > 1) {
            hwdtb_report_err(node, "Timer IRQs are not expected to connect to "
                                   "multiple IRQ controllers");
            return;
        }

        target = &g_array_index(conn->targets, HwDtbConnectionTarget, 0);

        if (target->target != first_target->target) {
            hwdtb_report_err(node, "All timer IRQs are expected to connect to "
                                   "the same IRQ controller");
            return;
        }
    }

    /* We can now assume the unique target is a GIC */
    gic = first_target->target;
    gic_obj = hwdtb_get_obj(gic);

    first_cpu_idx = object_property_get_uint(gic_obj, "first-cpu-idx", &err);

    if (err) {
        err = NULL;
        first_cpu_idx = 0;
    }

    num_cpu = object_property_get_uint(gic_obj, "num-cpu", &err);

    if (err) {
        hwdtb_report_err(node, "IRQ controller is not a GIC (missing num-cpu)");
        return;
    }

    num_irq = object_property_get_uint(gic_obj, "num-irq", &err) - GIC_INTERNAL;

    if (err) {
        hwdtb_report_err(node, "IRQ controller is not a GIC (missing num-irq)");
        return;
    }

    has_security_ext = !!object_property_get_bool(gic_obj,
                                                  "has-security-extensions",
                                                  &err);

    if (err) {
        hwdtb_report_err(node, "IRQ controller is not a GIC "
                         "(missing has-security-extensions)");
        return;
    }

    for (i = 0; i < num_cpu; i++) {
        CPUState *cpu = qemu_get_cpu(first_cpu_idx + i);
        size_t j = 0;

        if (cpu == NULL) {
            hwdtb_report_err(node, "Cannot get CPU with index %zu", first_cpu_idx + i);
            return;
        }

        hwdtb_node_foreach_connection(conn, node, HWDTB_CON_INTERRUPT) {
            static const int TUPLE_IDX_TO_CPU_GPIO[] = {
                [0] = 3, /* GTIMER_SEC */
                [1] = 0, /* GTIMER_PHYS */
                [2] = 1, /* GTIMER_VIRT */
                [3] = 2, /* GTIMER_HYP */
            };

            HwDtbConnectionTarget *target;
            uint32_t ppi_idx, gic_irq_idx, cpu_irq_idx;
            qemu_irq irq;
            g_autofree char *cpu_path = NULL;

            if (j > (3 + has_security_ext)) {
                break;
            }

            target = &g_array_index(conn->targets, HwDtbConnectionTarget, 0);

            if ((g_array_index(target->tuple, uint32_t, 0) & 0xff) != 1) {
                hwdtb_report_err(node, "Timer IRQ is not a PPI");
                return;
            }

            ppi_idx = g_array_index(target->tuple, uint32_t, 1);
            gic_irq_idx = num_irq + i * GIC_INTERNAL + GIC_NR_SGIS + ppi_idx;
            cpu_irq_idx = TUPLE_IDX_TO_CPU_GPIO[j + !has_security_ext];

            irq = qdev_get_gpio_in(DEVICE(gic_obj), gic_irq_idx);
            qdev_connect_gpio_out(DEVICE(cpu), cpu_irq_idx, irq);

            cpu_path = object_get_canonical_path(OBJECT(cpu));
            trace_hwdtb_node_legacy_armv8_timer_connect(cpu_path, cpu_irq_idx,
                                                        gic->path, gic_irq_idx);

            j++;
        }
    }
}

/*
 * The PMC Sysmon model was using a "array of links" property for the satellites
 * in the legacy code. Current QEMU upstream does not support array of links
 * properties. The property has been replaced with an array of uint32. It is
 * filled during DTB parsing with the satellites handles. This callback resolves
 * the handles and fills the ams_sat array with the corresponding objects.
 */
void hwdtb_quirk_pmc_sysmon_resolve_phandles(HwDtbNode *node, void *opaque)
{
    PMCSysMon *pmc_sysmon = PMC_SYSMON(hwdtb_get_obj(node));
    size_t i;

    pmc_sysmon->ams_sat = g_new0(Object *, pmc_sysmon->ams_sat_len);

    for (i = 0; i < pmc_sysmon->ams_sat_len; i++) {
        uint32_t phandle = pmc_sysmon->ams_sat_phandle[i];
        HwDtbNode *link_node = hwdtb_get_node_by_phandle(node->hwdtb, phandle);

        if (link_node == NULL) {
            pmc_sysmon->ams_sat[i] = NULL;
            continue;
        }

        pmc_sysmon->ams_sat[i] = hwdtb_get_obj(link_node);
    }
}
