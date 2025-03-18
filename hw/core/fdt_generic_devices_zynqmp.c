#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/fdt_generic.h"
#include "sysemu/device_tree.h"

static bool is_zynqmp(FDTMachineInfo *fdti)
{
    char root[DT_PATH_LENGTH];
    char *compat;
    int len;

    qemu_devtree_get_root_node(fdti->fdt, root);
    compat = qemu_fdt_getprop(fdti->fdt, root, "compatible", &len, false, NULL);

    if (compat == NULL) {
        return false;
    }

    while (len) {
        if (!strcmp(compat, "xlnx,zynqmp")) {
            return true;
        }

        len -= strlen(compat) + 1;
        compat += strlen(compat) + 1;
    }

    return false;
}

/*
 * Add the gpi-sample-mask property on the gpi2 device if missing. This is to
 * ensure backward compatibility with older hwdtbs.
 *
 * This property ensures the STANDBYWFI signals from the APUs get sampled by the
 * iomodule GPI device. This is a workaround to a discrepancy between QEMU WFI
 * ARM instruction implementation and real hardware one.
 */
static int gpi2_set_sampling_prop(char *node_path, FDTMachineInfo *fdti,
                                  void *priv)
{
    const char *PROP = "xlnx,gpi-sample-mask";

    if (!is_zynqmp(fdti)) {
        return 1;
    }

    if (qemu_fdt_getprop(fdti->fdt, node_path, PROP, NULL, false, NULL)) {
        /* property already present on the node */
        return 1;
    }

    /* set the property and let the node be handled by generic code */
    qemu_fdt_setprop_cell(fdti->fdt, node_path, PROP, 0xf);

    return 1;
}

fdt_register_instance(gpi2_set_sampling_prop, "pmu_gpi@28");

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
