/*
 * HWDTB GPIO legacy resolution pass
 *
 * This pass happens just before the regular GPIO resolution pass. It tries to
 * resolve GPIOs using the legacy FDT_GENERIC_GPIO mapping
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/hwdtb.h"
#include "hw/fdt_generic_util.h"
#include "trace.h"

static const FDTGenericGPIOConnection *
find_legacy_gpio_mapping_in_set(const FDTGenericGPIOSet *set,
                                HwDtbConnectionKind kind, size_t idx,
                                bool client)
{
    const char *spec_extended;
    const FDTGenericGPIOConnection *map = NULL;

    spec_extended = hwdtb_conn_format_get_spec_extended(kind);

    while (set->names) {
        if (!strcmp(set->names->propname, spec_extended)) {
            map = set->gpios;
            break;
        }

        set++;
    }

    if (map == NULL) {
        return NULL;
    }

    while (map->name) {
        /* nul range are treated as one in legacy code */
        size_t range = map->range ?: 1;

        if (map->fdt_index <= idx &&
            (map->fdt_index + range) > idx) {
            return map;
        }

        map++;
    }

    return NULL;
}


static const FDTGenericGPIOConnection *
find_legacy_gpio_mapping(HwDtbNode *node, HwDtbConnectionKind kind, size_t idx,
                         bool client)
{
    FDTGenericGPIOClass *fdtggc;
    const FDTGenericGPIOSet *set;

    fdtggc = FDT_GENERIC_GPIO_GET_CLASS(hwdtb_get_obj(node));
    set = client ? fdtggc->client_gpios : fdtggc->controller_gpios;

    if (set == NULL) {
        return NULL;
    }

    return find_legacy_gpio_mapping_in_set(set, kind, idx, client);
}

static bool get_legacy_gpio_mapping(HwDtbNode *node, HwDtbConnectionKind kind,
                                    size_t idx, bool client,
                                    HwDtbResolvedGPIO *ret)
{
    const FDTGenericGPIOConnection *map = NULL;

    if (HWDTB_NODE_AS(node, FDT_GENERIC_GPIO)) {
        map = find_legacy_gpio_mapping(node, kind, idx, client);
    }

    if (map == NULL && client) {
        map = find_legacy_gpio_mapping_in_set(default_gpio_sets, kind, idx,
                                              client);
    }

    if (map == NULL) {
        return false;
    }

    ret->name = map->name;
    ret->idx = idx - map->fdt_index;

    return true;
}

static void resolve_gpio(HwDtbNode *node, HwDtbResolvedGPIO *gpio)
{
    if (hwdtb_node_has_gpio_input(node, gpio->name, gpio->idx)) {
        gpio->sta = HWDTB_GPIO_INPUT;
    } else if (hwdtb_node_has_gpio_output(node, gpio->name, gpio->idx)) {
        gpio->sta = HWDTB_GPIO_OUTPUT;
    } else {
        qemu_log_mask(LOG_FDT,
                      "%s: legacy GPIO map resolution failure: GPIO [%s, %zu] "
                      "not found\n", node->path,
                      gpio->name ?: "unnamed-gpio", gpio->idx);
        gpio->sta = HWDTB_GPIO_RESOLUTION_FAILURE;
    }
}

static void resolve_client_legacy_gpio_map(HwDtbNode *node,
                                           HwDtbConnection *conn)
{
    if (hwdtb_gpio_is_resolved(&conn->gpio)) {
        return;
    }

    if (!get_legacy_gpio_mapping(node, conn->kind, conn->idx, true,
                                 &conn->gpio)) {
        return;
    }

    resolve_gpio(node, &conn->gpio);
    trace_hwdtb_node_gpio_resolve_legacy_map(node->path, conn->idx,
                                             hwdtb_gpio_get_resolution_str(&conn->gpio),
                                             conn->gpio.name, conn->gpio.idx);
}

static void resolve_controller_legacy_gpio_map(HwDtbConnectionTarget *target,
                                               HwDtbConnectionKind kind)
{
    size_t idx;

    if (hwdtb_gpio_is_resolved(&target->gpio)) {
        return;
    }

    /*
     * Only one cell supported. Some hwdtbs have two cells with the second equal
     * to 0. Ignore it.
     */
    switch (target->tuple->len) {
    case 1:
        break;

    case 2:
        if (g_array_index(target->tuple, uint32_t, 1) != 0) {
            return;
        }
        break;

    default:
        return;
    }

    idx = g_array_index(target->tuple, uint32_t, 0);

    /*
     * When connecting GPIOs in hwdtb, there is no way to specify the GPIO
     * namespace of the controller. Interrupts are a special case since they can
     * be handled using FDT_GENERIC_INTC interfaces.
     */
    if (kind != HWDTB_CON_INTERRUPT) {
        kind = HWDTB_CON_GPIO;
    }

    if (!get_legacy_gpio_mapping(target->target, kind, idx, false,
                                 &target->gpio)) {
        return;
    }

    resolve_gpio(target->target, &target->gpio);
    trace_hwdtb_node_gpio_resolve_legacy_map(target->target->path, idx,
                                             hwdtb_gpio_get_resolution_str(&target->gpio),
                                             target->gpio.name,
                                             target->gpio.idx);
}

static void conn_resolve_legacy_gpio_map(HwDtbNode *node, HwDtbConnection *conn)
{
    size_t i;

    resolve_client_legacy_gpio_map(node, conn);

    for (i = 0; i < conn->targets->len; i++) {
        HwDtbConnectionTarget *target;

        target = &g_array_index(conn->targets, HwDtbConnectionTarget, i);
        resolve_controller_legacy_gpio_map(target, conn->kind);
    }
}

static void gpio_legacy_resolve_conns(HwDtbNode *node,
                                      HwDtbConnectionKind kind)
{
    HwDtbConnection *conn;

    hwdtb_node_foreach_connection(conn, node, kind) {
        conn_resolve_legacy_gpio_map(node, conn);
    }
}

static void node_gpio_legacy_resolve(HwDtbNode *node)
{
    size_t i;

    for (i = 0; i < HWDTB_NUM_GPIO_CON; i++) {
        gpio_legacy_resolve_conns(node, i);
    }
}

void hwdtb_gpio_legacy_resolve(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_gpio_legacy_resolve);
}
