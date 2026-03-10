/*
 * HWDTB GPIO resolve pass
 *
 * This pass tries to resolve GPIO connections by identifying inputs and outputs
 * on the devices.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/hwdtb.h"
#include "hw/sysbus.h"
#include "hw/fdt_generic_util.h"
#include "trace.h"

static const char *CONN_GPIO_NAMESPACE[] = {
    [HWDTB_CON_INTERRUPT] = SYSBUS_DEVICE_GPIO_IRQ,
    [HWDTB_CON_GPIO] = NULL,
    [HWDTB_CON_POWER_GPIO] = "power",
    [HWDTB_CON_RESET_GPIO] = "reset",
    [HWDTB_CON_INTERRUPT_GPIO] = NULL,
    [HWDTB_CON_ERROR_OUT_GPIO] = NULL,
    [HWDTB_CON_PWR_STATE_GPIO] = NULL,
};

static void conn_resolve_client(HwDtbNode *node, HwDtbConnection *conn)
{
    const char *name;
    size_t idx;

    if (hwdtb_gpio_is_resolved(&conn->gpio)) {
        return;
    }

    name = conn->name ?: CONN_GPIO_NAMESPACE[conn->kind];

    /*
     * When a gpio namespace is provided using a *-names property, the index is
     * always 0. The cell index in the specifier property is ignored.
     *
     * E.g.:
     *    foo {
     *       gpios = <&bar 0 &baz 3>;
     *       gpio-names = "foo", "bar";
     *    }
     *
     * In this case we should look for GPIO foo[0] and bar[0] on the foo node.
     */
    idx = conn->name ? 0 : conn->idx;

    if (hwdtb_node_has_gpio_output(node, name, idx)) {
        conn->gpio.sta = HWDTB_GPIO_OUTPUT;
    } else if (hwdtb_node_has_gpio_input(node, name, idx)) {
        /*
         * -- Legacy --
         * Reversed direction
         */
        conn->gpio.sta = HWDTB_GPIO_INPUT;
    } else {
        qemu_log_mask(LOG_FDT,
                      "%s: GPIO resolution failure: GPIO [%s, %zu] not found\n",
                      node->path, name, idx);
        conn->gpio.sta = HWDTB_GPIO_RESOLUTION_FAILURE;
        return;
    }

    conn->gpio.idx = idx;
    conn->gpio.name = name;

    trace_hwdtb_node_gpio_resolve(node->path,
                                  hwdtb_gpio_get_resolution_str(&conn->gpio),
                                  conn->gpio.name ?: "unnamed-gpio",
                                  conn->gpio.idx);
}

static void target_resolve(HwDtbConnection *conn, HwDtbConnectionTarget *target)
{
    size_t idx;

    if (hwdtb_gpio_is_resolved(&target->gpio)) {
        return;
    }

    if (conn->kind == HWDTB_CON_INTERRUPT &&
        HWDTB_NODE_AS(target->target, FDT_GENERIC_INTC)) {
        size_t i;

        trace_hwdtb_node_gpio_resolve_legacy_intc(target->target->path);
        target->gpio.sta = HWDTB_GPIO_LEGACY_INTC;

        return;
    }

    /*
     * -- Legacy --
     * #gpio-cells > 1 does not make much sense in the hwdtb usecase. However in
     * some legacy dtbs, some gpio-controller nodes have a #gpio-cells = 2. The
     * second element of the tuple is always 0 and is ignored by the legacy
     * fdt_generic code.
     *
     * We want to get rid of those and only support #gpio-cells = <1>
     */
    if (target->tuple->len == 0) {
        target->gpio.sta = HWDTB_GPIO_RESOLUTION_FAILURE;
        return;
    }

    idx = g_array_index(target->tuple, uint32_t, 0);

    if (hwdtb_node_has_gpio_input(target->target, NULL, idx)) {
        target->gpio.sta = HWDTB_GPIO_INPUT;
    } else if (hwdtb_node_has_gpio_output(target->target, NULL, idx)) {
        /*
         * -- Legacy --
         * Reversed direction
         */
        target->gpio.sta = HWDTB_GPIO_OUTPUT;
    } else {
        qemu_log_mask(LOG_FDT,
                      "%s: GPIO resolution failure: GPIO "
                      "[unnamed-gpio, %zu] not found\n",
                      target->target->path, idx);
        target->gpio.sta = HWDTB_GPIO_RESOLUTION_FAILURE;
        return;
    }

    target->gpio.idx = idx;
    target->gpio.name = NULL; /* unnamed gpio */

    trace_hwdtb_node_gpio_resolve(target->target->path,
                                  hwdtb_gpio_get_resolution_str(&target->gpio),
                                  "unnamed-gpio",
                                  target->gpio.idx);
}

/*
 * -- Legacy --
 * Since GPIO connections do not describe their direction, we can end up in
 * ambiguous cases where a GPIO exists both as an input and an output on a given
 * device.
 *
 * This function tries to resolve those cases by:
 *    - finding connections resolved as in - in or out - out,
 *    - trying to resolve them as in - out or out - in connections.
 *
 * Note that if a connection can both be resolved as in - out and out - in,
 * there is not much we can do in term of guessing what the user wanted in the
 * first place...
 *
 * This heuristic can go away once we deprecate those non-directionnal
 * connections in hwdtbs.
 */
static void conn_legacy_deambiguous(HwDtbNode *node, HwDtbConnection *conn)
{
    HwDtbConnectionTarget *target;

    if (conn->targets->len != 1) {
        /*
         * Don't bother with multiple targets connections. Those are limited to
         * HWDTB_CON_INTERRUPT connections which don't have this direction
         * resolution issue (they are always described in the out -> in
         * direction in dtbs)
         */
        return;
    }

    target = &g_array_index(conn->targets, HwDtbConnectionTarget, 0);

    if ((conn->gpio.sta == HWDTB_GPIO_OUTPUT) &&
        (target->gpio.sta == HWDTB_GPIO_OUTPUT)) {
        /* try with client as an input */
        if (hwdtb_node_has_gpio_input(node, conn->gpio.name, conn->gpio.idx)) {
            trace_hwdtb_node_legacy_gpio_deambiguous(
                node->path, conn->gpio.name ?: "unnamed-gpio", conn->gpio.idx,
                "input");
            conn->gpio.sta = HWDTB_GPIO_INPUT;
        }
    }

    if ((conn->gpio.sta == HWDTB_GPIO_INPUT) &&
        (target->gpio.sta == HWDTB_GPIO_INPUT)) {
        /* try with controller as an input */
        if (hwdtb_node_has_gpio_output(target->target, target->gpio.name,
                                       target->gpio.idx)) {
            trace_hwdtb_node_legacy_gpio_deambiguous(
                target->target->path, target->gpio.name ?: "unnamed-gpio",
                target->gpio.idx, "output");
            target->gpio.sta = HWDTB_GPIO_OUTPUT;
        }
    }

}

static void conn_resolve(HwDtbNode *node, HwDtbConnection *conn)
{
    size_t i;

    conn_resolve_client(node, conn);

    for (i = 0; i < conn->targets->len; i++) {
        HwDtbConnectionTarget *target;

        target = &g_array_index(conn->targets, HwDtbConnectionTarget, i);
        target_resolve(conn, target);
    }

    conn_legacy_deambiguous(node, conn);
}

static void node_conns_resolve(HwDtbNode *node, HwDtbConnectionKind kind)
{
    HwDtbConnection *conn;

    hwdtb_node_foreach_connection(conn, node, kind) {
        conn_resolve(node, conn);
    }
}

static void hwdtb_node_gpio_resolve(HwDtbNode *node)
{
    HwDtbNode *child;
    size_t i;

    for (i = 0; i < HWDTB_NUM_GPIO_CON; i++) {
        node_conns_resolve(node, i);
    }

    hwdtb_node_foreach_child(child, node) {
        hwdtb_node_gpio_resolve(child);
    }
}

void hwdtb_gpio_resolve(HwDtb *hwdtb)
{
    hwdtb_node_gpio_resolve(hwdtb->root);
}
