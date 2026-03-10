/*
 * HWDTB GPIO connections
 *
 * This unit takes care of wiring devices GPIOs according to the previous GPIO
 * resolution passes.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/hwdtb.h"
#include "hw/sysbus.h"
#include "hw/core/split-irq.h"
#include "hw/or-irq.h"
#include "hw/qdev-properties.h"
#include "hw/fdt_generic_util.h"
#include "error.h"
#include "trace.h"

static DeviceState *create_child_irq_dev_str_idx(HwDtbNode *node,
                                                 const char *type,
                                                 const char *prefix,
                                                 const char *idx,
                                                 size_t num_lines)
{
    DeviceState *dev = NULL;
    g_autofree char *name;

    name = g_strdup_printf("%s-irq[%s]", prefix, idx);
    dev = qdev_new(type);

    hwdtb_node_add_child_obj(node, name, OBJECT(dev));
    qdev_prop_set_uint16(dev, "num-lines", num_lines);
    qdev_realize_and_unref(dev, NULL, &error_abort);

    return dev;
}

static DeviceState *create_child_or_gate_tuple_idx(HwDtbNode *node,
                                                   const GArray *idx,
                                                   size_t num_lines)
{
    g_autoptr(GString) idx_str = g_string_new("");

    hwdtb_str_append_tuple(idx_str, idx);
    return create_child_irq_dev_str_idx(node, TYPE_OR_IRQ, "or", idx_str->str,
                                        num_lines);
}

static DeviceState *create_child_split_irq(HwDtbNode *node, const char *name,
                                           size_t idx,
                                           size_t num_lines)
{
    g_autofree char *idx_str;

    idx_str = g_strdup_printf("%s[%zu]", name, idx);
    return create_child_irq_dev_str_idx(node, TYPE_SPLIT_IRQ, "split", idx_str,
                                        num_lines);
}

static qemu_irq hwdtb_dev_get_gpio_in_raw(HwDtbConnectionTarget *target,
                                          HwDtbConnectionKind kind,
                                          GString *descr)
{
    DeviceState *dev;

    switch (target->gpio.sta) {
    case HWDTB_GPIO_UNRESOLVED:
        /* The GPIO reolution passes are expected to be done at this point */
        g_assert_not_reached();

    case HWDTB_GPIO_LEGACY_INTC:
        /* TODO */
        return NULL;

    case HWDTB_GPIO_RESOLUTION_FAILURE:
        g_string_assign(descr, "GPIO resolution has failed");
        return NULL;

    case HWDTB_GPIO_INPUT:
        dev = HWDTB_NODE_AS(target->target, DEVICE);

        if (dev == NULL) {
            g_string_assign(descr, "node object is not a device");
            return NULL;
        }

        if (target->gpio.name) {
            g_string_printf(descr, "gpio-in[%s, %zu]", target->gpio.name,
                            target->gpio.idx);
        } else {
            g_string_printf(descr, "gpio-in[%zu]", target->gpio.idx);
        }

        return qdev_get_gpio_in_named(dev, target->gpio.name, target->gpio.idx);

    case HWDTB_GPIO_OUTPUT:
        g_string_assign(descr, "expecting an input GPIO");
        return NULL;

    default:
        g_assert_not_reached();
    }
}

/*
 * If the target has multiple connections on the same GPIO, return a free OR
 * gate input for this GPIO. Otherwise, return the device GPIO itself.
 */
static qemu_irq hwdtb_dev_get_gpio_in(HwDtbConnectionTarget *target,
                                      HwDtbConnectionKind kind,
                                      GString *descr)
{
    HwDtbRegisteredGPIO *input;
    qemu_irq ret;

    input = hwdtb_node_get_registered_gpio(target->target, &target->gpio);

    if ((input->gate == NULL) && (input->num_conn == 1)) {
        return hwdtb_dev_get_gpio_in_raw(target, kind, descr);
    }

    if (input->failure) {
        g_string_assign(descr, input->cached_descr->str);
        return NULL;
    }

    if (input->gate == NULL) {
        DeviceState *or;
        qemu_irq in;

        in = hwdtb_dev_get_gpio_in_raw(target, kind, descr);

        if (in == NULL) {
            input->failure = true;
            input->cached_descr = g_string_new(descr->str);
            return NULL;
        }

        or = create_child_or_gate_tuple_idx(target->target, target->tuple,
                                            input->num_conn);
        qdev_connect_gpio_out(or, 0, in);

        input->gate = or;
        input->cached_descr = g_string_new(descr->str);
    }

    ret = qdev_get_gpio_in(input->gate, input->num_conn - 1);
    g_string_assign(descr, input->cached_descr->str);
    input->num_conn--;

    return ret;
}

static bool dev_try_connect_gpio_out_named(HwDtbNode *node,
                                           HwDtbResolvedGPIO *gpio,
                                           qemu_irq irq, GString *descr)
{
    g_assert(gpio->sta == HWDTB_GPIO_OUTPUT);

    if (gpio->name && !strcmp(gpio->name, SYSBUS_DEVICE_GPIO_IRQ)) {
        SysBusDevice *sbd = HWDTB_NODE_AS(node, SYS_BUS_DEVICE);

        if (sbd == NULL) {
            g_string_assign(descr, "node object is not a sysbus device");
            return false;
        }

        sysbus_connect_irq(sbd, gpio->idx, irq);
    } else {
        DeviceState *dev = HWDTB_NODE_AS(node, DEVICE);

        if (dev == NULL) {
            g_string_assign(descr, "node object is not a device");
            return false;
        }

        qdev_connect_gpio_out_named(dev, gpio->name, gpio->idx, irq);
    }

    if (gpio->name) {
        g_string_printf(descr, "gpio-out[%s, %zu]", gpio->name, gpio->idx);
    } else {
        g_string_printf(descr, "gpio-out[%zu]", gpio->idx);
    }
    return true;
}

static bool hwdtb_dev_connect_gpio_out_raw(HwDtbNode *node,
                                           HwDtbConnection *conn, qemu_irq irq,
                                           GString *descr)
{
    switch (conn->gpio.sta) {
    case HWDTB_GPIO_UNRESOLVED:
        /* The GPIO reolution passes are expected to be done at this point */
        g_assert_not_reached();

    case HWDTB_GPIO_LEGACY_INTC:
        /* Not a valid resolution for client connections */
        g_assert_not_reached();

    case HWDTB_GPIO_RESOLUTION_FAILURE:
        /* Handled in callers */
        g_assert_not_reached();

    case HWDTB_GPIO_INPUT:
        g_string_assign(descr, "expecting an output GPIO");
        return false;

    case HWDTB_GPIO_OUTPUT:
        return dev_try_connect_gpio_out_named(node, &conn->gpio, irq, descr);

    default:
        g_assert_not_reached();
    }
}

static bool hwdtb_dev_connect_gpio_out(HwDtbNode *node, HwDtbConnection *conn,
                                       qemu_irq irq, GString *descr)
{
    HwDtbRegisteredGPIO *output;

    output = hwdtb_node_get_registered_gpio(node, &conn->gpio);

    if ((output->gate == NULL) && (output->num_conn == 1)) {
        return hwdtb_dev_connect_gpio_out_raw(node, conn, irq, descr);
    }

    if (output->failure) {
        g_string_assign(descr, output->cached_descr->str);
        return false;
    }

    if (output->gate == NULL) {
        qemu_irq in;

        output->gate = create_child_split_irq(node, conn->gpio.name,
                                              conn->gpio.idx, output->num_conn);

        in = qdev_get_gpio_in(output->gate, 0);
        if (!hwdtb_dev_connect_gpio_out_raw(node, conn, in, descr)) {
            output->failure = true;
        }

        output->cached_descr = g_string_new(descr->str);

        if (output->failure) {
            return false;
        }
    }

    qdev_connect_gpio_out(output->gate, output->num_conn - 1, irq);
    g_string_assign(descr, output->cached_descr->str);
    output->num_conn--;

    return true;
}

static void dev_connect_target(HwDtbNode *node, HwDtbConnection *conn,
                               size_t target_idx)
{
    g_autoptr(GString) node_descr = g_string_new("");
    g_autoptr(GString) target_descr = g_string_new("");
    HwDtbConnectionTarget *target;
    qemu_irq irq;

    target = &g_array_index(conn->targets, HwDtbConnectionTarget, target_idx);

    if (target->gpio.sta == HWDTB_GPIO_RESOLUTION_FAILURE) {
        g_autoptr(GString) tuple_str = g_string_new("");

        hwdtb_str_append_tuple(tuple_str, target->tuple);
        hwdtb_report_err(target->target,
                         HWDTB_ERR2(GPIO_GET_INPUT, GPIO_REASON_RES_FAILURE),
                         tuple_str->str);
        return;
    }

    irq = hwdtb_dev_get_gpio_in(target, conn->kind, target_descr);

    if (!irq) {
        g_autoptr(GString) tuple_str = g_string_new("");

        hwdtb_str_append_tuple(tuple_str, target->tuple);
        hwdtb_report_err(node, HWDTB_ERR_GPIO_GET_INPUT "%s",
                         tuple_str->str, target_descr->str);
        return;
    }

    if (!hwdtb_dev_connect_gpio_out(node, conn, irq, node_descr)) {
        hwdtb_report_err(node,
                         HWDTB_ERR_GPIO_CONNECT_OUTPUT "%s",
                         conn->name ?: "unnamed-gpio", conn->idx,
                         node_descr->str);
        return;
    }

    trace_hwdtb_node_connect_gpio(node->path, node_descr->str,
                                  target->target->path, target_descr->str);
}

static void conn_connect_one(HwDtbNode *node, HwDtbConnection *conn)
{
    size_t i;

    if (conn->targets->len == 0) {
        hwdtb_report_err(node, HWDTB_ERR2(GPIO_CONNECT_OUTPUT,
                                          GPIO_REASON_MAP_EXPANSION_FAILED),
                         conn->name ?: "unnamed-gpio", conn->idx);
        return;
    }

    if (conn->gpio.sta == HWDTB_GPIO_RESOLUTION_FAILURE) {
        hwdtb_report_err(node,
                         HWDTB_ERR2(GPIO_CONNECT_OUTPUT,
                                    GPIO_REASON_RES_FAILURE),
                         conn->name ?: "unnamed-gpio", conn->idx);
        return;
    }

    for (i = 0; i < conn->targets->len; i++) {
        dev_connect_target(node, conn, i);
    }
}

static void node_perform_connections(HwDtbNode *node,
                                     HwDtbConnectionKind kind)
{
    HwDtbConnection *conn;

    hwdtb_node_foreach_connection(conn, node, kind) {
        conn_connect_one(node, conn);
    }
}

static void hwdtb_connect_dev_gpios(HwDtbNode *node)
{
    size_t i;

    for (i = 0; i < HWDTB_NUM_GPIO_CON; i++) {
        node_perform_connections(node, i);
    }
}

void hwdtb_connect_gpios(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, hwdtb_connect_dev_gpios);
}
