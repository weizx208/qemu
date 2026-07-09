/*
 * HWDTB character device bindings
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/hwdtb.h"
#include "qom/object.h"
#include "system/system.h"
#include "hw/qdev-properties.h"
#include "chardev/char.h"
#include "hw/remote-port.h"
#include "trace.h"

static Chardev *get_next_chardev(HwDtbNode *node)
{
    HwDtb *hwdtb = node->hwdtb;

    /*
     * Skip chardevs already explicitly taken by other devices (the ones that
     * have a chardev = "serialx" property in the hwdtb). We rely on this manual
     * enumeration because QEMU does not offer sufficient introspection
     * mechanism to know if a chardev is free or not. Relying on chardev->be
     * does not work for muxes and the mux implementation details are private.
     */
    while ((hwdtb->next_serial_hd < 32)
           && (1 << hwdtb->next_serial_hd) & hwdtb->reserved_serial_hd) {
        hwdtb->next_serial_hd++;
    }

    if (hwdtb->next_serial_hd >= 32) {
        return NULL;
    }

    return serial_hd(hwdtb->next_serial_hd++);
}

static void node_attach_char_dev(HwDtbNode *node)
{
    const char *chardev_str;
    Chardev *chardev;
    Object *obj;
    DeviceState *dev;

    dev = HWDTB_NODE_AS(node, DEVICE);

    if (!dev) {
        return;
    }

    obj = hwdtb_get_obj(node);

    /*
     * Legacy fdt_generic code auto-connects character backends to device having
     * a "chardev" property.
     */
    if (!object_property_find(obj, "chardev")) {
        return;
    }

    /* It also explicitly skips remote port devices... */
    if (HWDTB_NODE_AS(node, REMOTE_PORT)) {
        return;
    }

    chardev_str = object_property_get_str(obj, "chardev", NULL);

    if (chardev_str == NULL) {
        /* Not a string property? Skip it. */
        return;
    }

    if (*chardev_str) {
        /* Already set, nothing to do. */
        return;
    }

    chardev = get_next_chardev(node);

    while (chardev) {
        /*
         * Cannot use qdev_prop_set_chr here because it calls
         * object_property_set_str with &error_abort.
         *
         * Also, cannot determine if chardev is free before trying to set it
         * because mux chardevs details are private.
         */
        if (object_property_set_str(obj, "chardev", chardev->label, NULL)) {
            trace_hwdtb_node_set_prop_chr(node->path, chardev->label);
            return;
        }

        chardev = get_next_chardev(node);
    }

    trace_hwdtb_node_set_prop_chr_failure(node->path);
}

void hwdtb_attach_char_devs(HwDtb *hwdtb)
{
    hwdtb_walk(hwdtb, node_attach_char_dev);
}
