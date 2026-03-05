/*
 * HWDTB sub-system
 *
 * Construct a QEMU machine from a "hwdtb" description.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_HWDTB_H
#define QEMU_HWDTB_H

#include "qemu/queue.h"
#include "qom/object.h"
#include <libfdt.h>

typedef struct HwDtb HwDtb;
typedef struct HwDtbNode HwDtbNode;

typedef enum HwDtbRegEntryKind {
    HWDTB_REG_ADDR,
    HWDTB_REG_SIZE,
    HWDTB_REG_BUS,
    HWDTB_REG_PRIO,

    HWDTB_NUM_REG_KIND
} HwDtbRegEntryKind;

/**
 * One entry of any kind.
 *
 * @val: the value, at most two fdt cells (64 bits)
 * @valid: the corresponding kind was found in the tuple. If false this entry
 *         must be ignored.
 */
typedef struct HwDtbRegEntry {
    uint64_t val;
    bool valid;
} HwDtbRegEntry;

/**
 * A <address size bus prio> tuple, not necessarily with all values valid.
 *
 * @target: in case of a reg-extended property, contains the phandle specified
 *          as the first value in the tuple
 * @extended: true if the parsed property was reg-extended. see @target.
 */
typedef struct HwDtbRegTuple {
    HwDtbRegEntry entry[HWDTB_NUM_REG_KIND];

    size_t idx;
    HwDtbNode *target;
    bool extended;

    QSIMPLEQ_ENTRY(HwDtbRegTuple) link;
} HwDtbRegTuple;

/**
 * Connection kind
 *
 * %HWDTB_CON_INTERRUPT: interrupts = <...>
 * %HWDTB_CON_GPIO: gpios = <...>
 * %HWDTB_CON_CLOCK: clocks = <...>
 *
 * The rest are legacy kinds that are eventually handled as GPIOs. @see parse.c
 * for the connection descriptors.
 */
typedef enum HwDtbConnectionKind {
    HWDTB_CON_INTERRUPT,
    HWDTB_CON_GPIO,
    HWDTB_CON_POWER_GPIO,
    HWDTB_CON_RESET_GPIO,
    HWDTB_CON_INTERRUPT_GPIO,

    /* - Legacy -- pmu_global */
    HWDTB_CON_ERROR_OUT_GPIO,
    HWDTB_CON_PWR_STATE_GPIO,

    HWDTB_NUM_GPIO_CON,

    HWDTB_CON_CLOCK = HWDTB_NUM_GPIO_CON,

    HWDTB_NUM_CON
} HwDtbConnectionKind;

/**
 * A connection target
 *
 * Used to describe the target of a connection
 *
 * @node the node targeted by the connection
 * @name the connection name on the target. Used only for clocks which have a
 *       `clock-output-names' property for that.
 * @tuple the connection tuple
 * @gpio When the connection refers to a GPIO (or an IRQ), this field describes
 *       the GPIO as resolved by the GPIO resolution pass. @see HwDtbResolvedGPIO.
 */
typedef struct HwDtbConnectionTarget {
    HwDtbNode *target;
    const char *name;
    GArray *tuple;
} HwDtbConnectionTarget;

/**
 * A parsed connection on a node. Can be a GPIO or a clock.
 *
 * @kind the connection kind
 * @name the parsed name of the connection. E.g., for GPIOs, this is specified
 *       using gpio-names in the DTB.
 * @idx the connection index in the connection tuple.
 * @targets an array of HwDtbConnectionTarget. Usually connections are 1-1 in a
 *          DTB. However hwdtb abuse the interrupt-map property to connect one
 *          output IRQ to multiple inputs.
 * @gpio When the connection refers to a GPIO (or an IRQ), this field describes
 *       the GPIO as resolved by the GPIO resolution pass. @see
 *       HwDtbResolvedGPIO.
 *
 * Example:
 *
 * foo {
 *     interrupt-extended = <&bar 0 1 2>;
 * };
 *
 * bar: bar {
 *     interrupt-controller;
 *     #interrupt-cells = <3>;
 * };
 *
 * This will create one connection of kind HWDTB_CON_INTERRUPT, with NULL name
 * (unspecified here, would have been specified using interrupt-names), and
 * index 0. This connection will have one target with tuple (0, 1, 2).
 */
typedef struct HwDtbConnection {
    HwDtbConnectionKind kind;
    size_t idx;
    const char *name;

    GArray *targets;

    QSIMPLEQ_ENTRY(HwDtbConnection) link;
} HwDtbConnection;

/**
 * HwDtbNode
 *
 * Describes a node from the hwdtb, with additional decorations filled during
 * the hwdtb creation.
 *
 * @offset offset in the fdt. Note that because offset are cached, fdt
 *         modifications are not permitted during hwdtb machine creation
 * @path full path in the fdt
 *
 * @reg_num_cells parsed #xxx-cells for the reg property. Inherits the parent
 *                value if unspecified by the node.
 * @reg parsed `reg' or `reg-extended' property
 *
 * @connection parsed connections
 *
 * @hwdtb the parent hwdtb
 * @parent the parent node in the tree
 *
 * @children head of the children list
 * @link entry in the children list
 */
struct HwDtbNode {
    int offset;
    char *path;
    int reg_num_cells[HWDTB_NUM_REG_KIND];
    QSIMPLEQ_HEAD(, HwDtbRegTuple) reg;

    QSIMPLEQ_HEAD(, HwDtbConnection) connection[HWDTB_NUM_CON];

    HwDtb *hwdtb;
    HwDtbNode *parent;
    QSIMPLEQ_HEAD(, HwDtbNode) children;
    QSIMPLEQ_ENTRY(HwDtbNode) link;
};

/**
 * The main HwDtb structure
 *
 * @machine the machine being constructed
 * @fdt the hwdtb being parsed
 * @root the root node
 *
 * @node_by_phandle hash table to retrieve a node given a phandle
 * @node_by_path hash table to retrieve a node given a full path
 */
struct HwDtb {
    MachineState *machine;
    void *fdt;
    HwDtbNode *root;

    GHashTable *node_by_phandle;
    GHashTable *node_by_path;
};

/**
 * hwdtb_create_machine
 *
 * Main hwdtb entry point. Create the machine described by @fdt.
 *
 * @machine the machine to populate
 * @fdt the flattened device tree representing the hwdtb machine
 *
 * @return the corresponding HwDtb structure.
 */
HwDtb *hwdtb_create_machine(MachineState *machine, void *fdt);

/**
 * hwdtb_create_machine_oneshot
 *
 * Same as hwdtb_create_machine but does not return the HwDtb structure.
 *
 * @machine the machine to populate
 * @fdt the flattened device tree representing the hwdtb machine
 */
void hwdtb_create_machine_oneshot(MachineState *machine, void *fdt);

/**
 * hwdtb_free
 *
 * Free a hwdtb created with hwdtb_create_machine. Can be called safely after
 * machine creation. The machine in itself is untouched.
 */
void hwdtb_free(HwDtb *hwdtb);

/*
 * Most of the remaining functions below are private to the hwdtb subsystem.
 * Some of them can be used outside after having called hwdtb_create_machine on
 * the returned HwDtb struct to retrieve information on the created machine.
 */

/* Entry points of the various passes */
void hwdtb_parse(HwDtb *hwdtb);

#define hwdtb_node_foreach_child(child, parent) \
    QSIMPLEQ_FOREACH((child), &((parent)->children), link)

const char *hwdtb_node_get_name(const HwDtbNode *node);

/* Low-level FDT properties parsing functions */
bool hwdtb_fdt_prop_parse_uint(const struct fdt_property *prop, size_t skip,
                               size_t max, uint64_t *ret, size_t *len);
bool hwdtb_fdt_prop_parse_uint64(const struct fdt_property *prop,
                                 uint64_t *ret);
bool hwdtb_fdt_prop_parse_uint32(const struct fdt_property *prop,
                                 uint32_t *ret);
bool hwdtb_fdt_prop_parse_nth_uint32(const struct fdt_property *prop,
                                     size_t idx, uint32_t *ret);
const char *hwdtb_fdt_prop_parse_string(const struct fdt_property *prop);
const char *hwdtb_fdt_prop_parse_next_string(const struct fdt_property *prop,
                                             const char *prev);
const struct fdt_property *hwdtb_node_get_prop(const HwDtbNode *node,
                                               const char *prop_name,
                                               size_t *len);


/**
 * hwdtb_node_has_prop
 *
 * @return true if @node has the @prop_name property.
 */
bool hwdtb_node_has_prop(const HwDtbNode *node, const char *prop_name);

/**
 * hwdtb_node_get_prop_uint
 *
 * Parse an unsigned integer property up to 64 bits. This function tries to
 * consume the whole property data up to 64 bits. If the property size is bigger
 * than 64 bits or if it is not a power of 2 then false is returned.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 * @ret a pointer on a uint64_t in which to store the parsed data
 *
 * @return true if the parsing was successful, false otherwise. If false is
 * returned, ret is left untouched.
 */
bool hwdtb_node_get_prop_uint(const HwDtbNode *node, const char *prop_name,
                              uint64_t *ret);

/**
 * hwdtb_node_get_prop_uint64
 *
 * Parse an unsigned integer of exactly 64 bits. This function tries to
 * consume the whole property data. If the property size is not 64 bits false is
 * returned.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 * @ret a pointer on a uint64_t in which to store the parsed data
 *
 * @return true if the parsing was successful, false otherwise. If false is
 * returned, ret is left untouched.
 */
bool hwdtb_node_get_prop_uint64(const HwDtbNode *node, const char *prop_name,
                                uint64_t *ret);

/**
 * hwdtb_node_get_prop_uint32
 *
 * Parse an unsigned integer of exactly 32 bits. This function tries to
 * consume the whole property data. If the property size is not 32 bits false is
 * returned.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 * @ret a pointer on a uint32_t in which to store the parsed data
 *
 * @return true if the parsing was successful, false otherwise. If false is
 * returned, ret is left untouched.
 */
bool hwdtb_node_get_prop_uint32(const HwDtbNode *node, const char *prop_name,
                                uint32_t *ret);

/**
 * hwdtb_node_get_prop_nth_uint32
 *
 * Parse an unsigned integer of exactly 32 bits at an arbitrary cell position in
 * a property. This function interprets the property data as a cell tuple. It
 * does not consume the whole property data. It locates and tries to parse a
 * cell at the given @idx index. If the remaining property size at the given
 * index is less then 32 bits, false is returned.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 * @ret a pointer on a uint32_t in which to store the parsed data
 *
 * @return true if the parsing was successful, false otherwise. If false is
 * returned, ret is left untouched.
 *
 * Example:
 *
 * foo {
 *     bar = <0xff 0x1234 0xaa>;
 *
 * The call:
 *    hwdtb_node_get_prop_nth_uint32(foo, "bar", 1, &ret);
 *
 * Would return true and ret will be 0x1234 after the call.
 */
bool hwdtb_node_get_prop_nth_uint32(const HwDtbNode *node,
                                    const char *prop_name, size_t idx,
                                    uint32_t *ret);

/**
 * hwdtb_node_get_prop_string
 *
 * Parse a string property. The parsing will fail if the last byte of the
 * property data is not '\0'.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 *
 * @return a pointer onto the first char of the string, or %NULL if the parsing
 * failed
 */
const char *hwdtb_node_get_prop_string(const HwDtbNode *node, const char *prop);

/**
 * hwdtb_node_get_prop_strings
 *
 * Parse a multi-strings property. This function acts as an iterator over the
 * strings of the property. The first time it is called @prev must be %NULL. In
 * subsequent calls @prev must have the value returned by the function during
 * the previous call. The iterations stop whenever this function returns %NULL
 *
 * The parsing will fail if the last byte of the property data is not '\0'.
 *
 * @node the node to find the property on
 * @prop_name the name of the property to parse
 * @prev previous pointer returned by this function
 *
 * @return a pointer onto the first char of the next string, or %NULL if the
 * parsing failed or if the end of the property has been reached.
 */
const char *hwdtb_node_get_prop_strings(const HwDtbNode *node, const char *prop,
                                        const char *prev);

/**
 * hwdtb_get_node_by_phandle
 *
 * Lookup the @node with the phandle @phandle
 *
 * @return the node with phandle @phandle, or %NULL if not found
 */
HwDtbNode *hwdtb_get_node_by_phandle(HwDtb *hwdtb, uint32_t phandle);

/**
 * hwdtb_get_node_by_path
 *
 * Lookup the @node at path @path in the fdt
 *
 * @return the node at @path, or %NULL if not found
 */
HwDtbNode *hwdtb_get_node_by_path(HwDtb *hwdtb, const char *path);

/**
 * hwdtb_node_reg_get_first
 *
 * Convenient function to retrieve the first tuple of the reg property of @node
 *
 * @return the first tuple of the @node's reg property, or %NULL if @node has no
 * parsed reg property
 */
HwDtbRegTuple *hwdtb_node_reg_get_first(HwDtbNode *node);

/**
 * hwdtb_node_reg_get_first_addr
 *
 * Convenient function to retrieve the address value in the first tuple of the
 * reg property of @node
 *
 * @node the node to query the address on
 * @ret a pointer to a uint64_t in which the address is written on success
 *
 * @return true if the first tuple exists and has a valid address value, false
 * otherwise. If false is returned, ret is left untouched.
 */
bool hwdtb_node_reg_get_first_addr(const HwDtbNode *node, uint64_t *ret);

/**
 * hwdtb_node_reg_get_first_size
 *
 * Convenient function to retrieve the address value in the first tuple of the
 * reg property of @node
 *
 * @node the node to query the address on
 * @ret a pointer to a uint64_t in which the address is written on success
 *
 * @return true if the first tuple exists and has a valid address value, false
 * otherwise. If false is returned, ret is left untouched.
 */
bool hwdtb_node_reg_get_first_size(const HwDtbNode *node, uint64_t *ret);

uint64_t hwdtb_reg_tuple_val_nofail(const HwDtbRegTuple *tuple,
                                    HwDtbRegEntryKind entry);
uint64_t hwdtb_reg_tuple_val_or(const HwDtbRegTuple *tuple,
                                HwDtbRegEntryKind entry, uint64_t def_value);
uint64_t hwdtb_reg_tuple_val_or_prop_or(HwDtbNode *node, const char *prop,
                                        HwDtbRegTuple *tuple,
                                        HwDtbRegEntryKind kind,
                                        uint64_t def_value);

#define hwdtb_node_foreach_reg_tuple(tuple_, node_) \
    QSIMPLEQ_FOREACH((tuple_), &((node_)->reg), link)

#define hwdtb_node_foreach_reg_tuple_safe(tuple_, node_, next_) \
    QSIMPLEQ_FOREACH_SAFE((tuple_), &((node_)->reg), link, next_)

#define hwdtb_node_foreach_connection(conn_, node_, kind_) \
    QSIMPLEQ_FOREACH((conn_), &((node_)->connection[kind_]), link)

#define hwdtb_node_foreach_connection_safe(conn_, node_, kind_, next_) \
    QSIMPLEQ_FOREACH_SAFE((conn_), &((node_)->connection[kind_]), link, next_)

void hwdtb_str_append_tuple(GString *str, const GArray *tuple);

#endif
