# Default configuration for riscv32-softmmu

# Uncomment the following lines to disable these optional devices:
# CONFIG_PCI_DEVICES=n
# CONFIG_TEST_DEVICES=n

# Boards are selected by default, uncomment to keep out of the build.
# CONFIG_SPIKE=n
# CONFIG_SIFIVE_E=n
# CONFIG_SIFIVE_U=n
# CONFIG_RISCV_VIRT=n
# CONFIG_OPENTITAN=n

# Xilinx
CONFIG_SSI=y
CONFIG_I2C=y
CONFIG_XILINX_AXI=y
CONFIG_XILINX_SPI=y
CONFIG_XILINX_SPIPS=y
CONFIG_PTIMER=y
CONFIG_CADENCE=y
CONFIG_REMOTE_PORT=y
