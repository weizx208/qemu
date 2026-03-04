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
 * @hwdtb the parent hwdtb
 * @parent the parent node in the tree
 *
 * @children head of the children list
 * @link entry in the children list
 */
struct HwDtbNode {
    int offset;
    char *path;

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

#endif
