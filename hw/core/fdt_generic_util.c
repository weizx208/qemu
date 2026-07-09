/*
 * Utility functions for fdt generic framework
 *
 * Copyright (c) 2009 Edgar E. Iglesias.
 * Copyright (c) 2009 Michal Simek.
 * Copyright (c) 2011 PetaLogix Qld Pty Ltd.
 * Copyright (c) 2011 Peter A. G. Crosthwaite <peter.crosthwaite@petalogix.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/fdt_generic_util.h"

static const TypeInfo fdt_generic_intc_info = {
    .name          = TYPE_FDT_GENERIC_INTC,
    .parent        = TYPE_INTERFACE,
    .class_size = sizeof(FDTGenericIntcClass),
};

static const TypeInfo fdt_generic_mmap_info = {
    .name          = TYPE_FDT_GENERIC_MMAP,
    .parent        = TYPE_INTERFACE,
    .class_size = sizeof(FDTGenericMMapClass),
};

static const TypeInfo fdt_generic_gpio_info = {
    .name          = TYPE_FDT_GENERIC_GPIO,
    .parent        = TYPE_INTERFACE,
    .class_size = sizeof(FDTGenericGPIOClass),
};

static void fdt_generic_intc_register_types(void)
{
    type_register_static(&fdt_generic_intc_info);
    type_register_static(&fdt_generic_mmap_info);
    type_register_static(&fdt_generic_gpio_info);
}

type_init(fdt_generic_intc_register_types)
