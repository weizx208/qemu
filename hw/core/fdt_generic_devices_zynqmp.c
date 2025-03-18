#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/fdt_generic.h"
#include "sysemu/device_tree.h"

static const TypeInfo fdt_qom_aliases[] = {
    {   .name = "arasan,sdhci-8.9a",        .parent = "xilinx.zynqmp-sdhci" },
    {   .name = "xlnx,xps-gpio-1.00.a",     .parent = "xlnx.axi-gpio"       },
    {   .name = "xlnx,axi-dpdma-1.0",       .parent = "xlnx.dpdma"          },
    {   .name = "xlnx,ps7-can-1.00.a",      .parent = "xlnx.zynqmp-can"     },
};

static void fdt_generic_register_types(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(fdt_qom_aliases); ++i) {
        type_register_static(&fdt_qom_aliases[i]);
    }
}

type_init(fdt_generic_register_types)
