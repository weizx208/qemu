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

#define TYPE_HWDTB_PROXY "hwdtb-proxy"
OBJECT_DECLARE_SIMPLE_TYPE(HwDtbProxyState, HWDTB_PROXY)

typedef enum HwDtbProxyFlags {
    HWDTB_PROXY_LOCAL_OBJ = (1 << 0), /* local object: object created by hwdtb subsystem */
    HWDTB_PROXY_PARENT_ON_OBJ = (1 << 1), /* when parenting on the proxy, parent on the aliased object instead */
} HwDtbProxyFlags;

struct HwDtbProxyState {
    Object parent;

    Object *proxy;
    HwDtbProxyFlags flags;

};

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

typedef enum HwDtbGPIOResolutionStatus {
    HWDTB_GPIO_UNRESOLVED,
    HWDTB_GPIO_INPUT,
    HWDTB_GPIO_OUTPUT,
    HWDTB_GPIO_LEGACY_INTC,
    HWDTB_GPIO_RESOLUTION_FAILURE,
} HwDtbGPIOResolutionStatus;

/**
 * A resolved GPIO
 *
 * Found in HwDtbConnection and HwDtbConnectionTarget structs. Describe the GPIO
 * after it has been resolved by the GPIO resolution pass.
 *
 * @sta the resolution status (unresolved, input, output, legacy intc or failure)
 * @name the QEMU GPIO namespace in which the GPIO can be found on the device
 * @idx the index of the GPIO in the GPIO namespace
 */
typedef struct HwDtbResolvedGPIO {
    HwDtbGPIOResolutionStatus sta;
    const char *name; /* GPIO namespace */
    size_t idx;
} HwDtbResolvedGPIO;

/**
 * A registered GPIO on a node
 *
 * Nodes use this struct in hash tables gpio_input and gpio_output (@see
 * HwDtbNode). Each time a GPIO is resolved on a device by the resolution pass,
 * it is registered in such a structure. This is used by the GPIO connection
 * pass to account for GPIOs with multiple connections. It will create the
 * corresponding gate (`or-irq', or `split-irq') whether this GPIO is an input
 * or an output.
 *
 * @num_conn number of connection to handle on this device's GPIO
 * @gate the created gate to handle the multiple connections
 * @cached_descr used by the connection pass for error and trace reporting
 * @failure used by the connection pass for error reporting
 */
typedef struct HwDtbRegisteredGPIO {
    size_t num_conn;
    DeviceState *gate; /* or-irq for inputs, split-irq for outputs */
    GString *cached_descr;
    bool failure;
} HwDtbRegisteredGPIO;

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
    HwDtbResolvedGPIO gpio;
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
    HwDtbResolvedGPIO gpio;

    GArray *targets;

    QSIMPLEQ_ENTRY(HwDtbConnection) link;
} HwDtbConnection;

typedef Object *(*HwDtbObjectFactory)(HwDtbNode *);

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
 * @oc Object class resolved from compatible string during the resolve pass
 * @factory Factory function resolved from compatible string during the resolve
 *          pass
 * @obj QOM object created during the creation pass
 *
 * @reg_num_cells parsed #xxx-cells for the reg property. Inherits the parent
 *                value if unspecified by the node.
 * @reg parsed `reg' or `reg-extended' property
 *
 * @connection parsed connections
 * @gpio_input hash table of HwDtbRegisteredGPIO. Register connections for input
 *             GPIOs on this node.
 * @gpio_output hash table of HwDtbRegisteredGPIO. Register connections for
 *              output GPIOs on this node.
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
    ObjectClass *oc;
    HwDtbObjectFactory factory;
    Object *obj;

    int reg_num_cells[HWDTB_NUM_REG_KIND];
    QSIMPLEQ_HEAD(, HwDtbRegTuple) reg;

    QSIMPLEQ_HEAD(, HwDtbConnection) connection[HWDTB_NUM_CON];
    GHashTable *gpio_input;
    GHashTable *gpio_output;

    HwDtb *hwdtb;
    HwDtbNode *parent;
    QSIMPLEQ_HEAD(, HwDtbNode) children;
    QSIMPLEQ_ENTRY(HwDtbNode) link;
};

/*
 * Convenient macro to retrieve the QOM object associated with a node, while
 * converting it to a specific QOM type. This returns NULL if the type
 * mismatches.
 */
#define HWDTB_NODE_AS(node_, qom_type_) \
    qom_type_(object_dynamic_cast(hwdtb_get_obj(node_), TYPE_ ## qom_type_))

/*
 * All the hwdtb passes. Callbacks can be registered for a node, before or after
 * a pass is run (@see hwdtb_node_register_callback)
 */
typedef enum HwDtbPass {
    HWDTB_PASS_INSTANTIATE,
    HWDTB_PASS_SET_PROPERTIES,
    HWDTB_PASS_CONNECT_CLOCK,
    HWDTB_PASS_REALIZE,
    HWDTB_PASS_MEM_MAP,
    HWDTB_PASS_RESOLVE_GPIO,
    HWDTB_PASS_CONNECT_GPIO,
    HWDTB_PASS_END,

    HWDTB_NUM_PASSES
} HwDtbPass;

typedef void (*HwDtbPassCallbackFn)(HwDtbNode *node, void *opaque);

typedef struct HwDtbPassCallback {
    HwDtbPassCallbackFn fn;
    HwDtbNode *node;
    void *opaque;
} HwDtbPassCallback;

/**
 * The main HwDtb structure
 *
 * @machine the machine being constructed
 * @fdt the hwdtb being parsed
 * @root the root node
 *
 * @node_by_phandle hash table to retrieve a node given a phandle
 * @node_by_path hash table to retrieve a node given a full path
 *
 * @cpu_clusters hash table of automatically created CPU clusters. Indexed by
 *               CPU type.
 * @next_cluster_id keep tracks of cluster IDs used by the hwdtb to not clash
 *                  with them with auto-clusters.
 *
 * @next_serial_hd next Chardev to return from the serial_hd() call
 * @reserved_serial_hd keep tracks of reserved serial_hds by the hwdtb
 *
 * @num_cpu_found number of CPU found. Used by Zynq7000 legacy code
 *
 * @callbacks registered callbacks for the various passes
 */
struct HwDtb {
    MachineState *machine;
    void *fdt;
    HwDtbNode *root;

    GHashTable *node_by_phandle;
    GHashTable *node_by_path;

    GHashTable *cpu_clusters;
    uint32_t next_cluster_id;

    size_t next_serial_hd;
    uint32_t reserved_serial_hd;

    size_t num_cpu_found;

    GArray *callbacks[HWDTB_NUM_PASSES];
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
void hwdtb_resolve(HwDtb *hwdtb);
void hwdtb_instantiate(HwDtb *hwdtb);
void hwdtb_set_properties(HwDtb *hwdtb);
void hwdtb_attach_block_devs(HwDtb *hwdtb);
void hwdtb_attach_net_devs(HwDtb *hwdtb);
void hwdtb_attach_char_devs(HwDtb *hwdtb);
void hwdtb_attach_remote_ports(HwDtb *hwdtb);
void hwdtb_connect_clocks(HwDtb *hwdtb);
void hwdtb_realize_devs(HwDtb *hwdtb);
void hwdtb_legacy_mmap_iface(HwDtb *hwdtb);
void hwdtb_mem_map_nodes(HwDtb *hwdtb);
void hwdtb_gpio_legacy_resolve(HwDtb *hwdtb);
void hwdtb_gpio_resolve(HwDtb *hwdtb);
void hwdtb_gpio_legacy_reverse(HwDtb *hwdtb);
void hwdtb_gpio_register(HwDtb *hwdtb);
void hwdtb_connect_gpios(HwDtb *hwdtb);

/*
 * hwdtb_walk
 *
 * Do a pre-order traversal of the @hwdtb tree, calling @fn for each node
 */
void hwdtb_walk(HwDtb *hwdtb, void (*fn)(HwDtbNode *node));

#define hwdtb_node_foreach_child(child, parent) \
    QSIMPLEQ_FOREACH((child), &((parent)->children), link)

const char *hwdtb_node_get_name(const HwDtbNode *node);

const char *hwdtb_compat_translate(const char *compat);
HwDtbObjectFactory hwdtb_get_factory_for_compat(const char *compat);
HwDtbObjectFactory hwdtb_get_default_factory(void);

/**
 * hwdtb_node_register_callback
 *
 * Register a callback for @node to be executed after @pass
 *
 * @node the node to call the callback for
 * @pass the pass after which the callback is called
 * @fn the callback
 * @opaque the opaque value passed to the callback
 */
void hwdtb_node_register_callback(HwDtbNode *node, HwDtbPass pass,
                                  HwDtbPassCallbackFn fn, void *opaque);

/**
 * hwdtb_node_register_callback_before
 *
 * Register a callback for @node to be executed before @pass
 * Pass must be HWDTB_PASS_SET_PROPERTIES or a later pass. It cannot be the
 * first pass (HWDTB_PASS_INSTANTIATE). If an action is required before
 * instantiation on a given node, this must be done through a factory function.
 *
 * @node the node to call the callback for
 * @pass the pass before which the callback is called
 *       (>= HWDTB_PASS_SET_PROPERTIES)
 * @fn the callback
 * @opaque the opaque value passed to the callback
 */
void hwdtb_node_register_callback_before(HwDtbNode *node, HwDtbPass pass,
                                         HwDtbPassCallbackFn fn, void *opaque);

void hwdtb_call_callbacks(HwDtb *hwdtb, HwDtbPass pass);

/**
 * hwdtb_create_proxy
 *
 * Create a proxy object for QOM object @obj
 *
 * A proxy object has a "proxy" link property pointing to @obj. It is used when
 * we want to have objects in the hwdtb tree that are not created by the hwdtb
 * itself. Since they are created outside of hwdtb context, they cannot be
 * parented in the hwdtb tree, hence this proxy object. The proxy object can be
 * parented as normal because it is created in the hwdtb context.
 *
 * Another use-case is when the object is supposed to be at some place in the
 * hwdtb tree but we need to parent them elsewhere in practice. This is the case
 * for the CPUs that must be parented to a CPU cluster to be associated with it.
 * In that case the CPU is a hwdtb object (so the HWDTB_PROXY_LOCAL_OBJ flag is
 * used), but elsewhere in the QOM tree.
 *
 * When querying the QOM object associated to a hwdtb node, use
 * hwdtb_get_obj(node) instead of reading node->obj directly. This function
 * handles proxy objects correctly.
 *
 * @obj the object to create a proxy for
 * @flags flags associated to the proxy
 *
 * @return the created proxy object
 */
Object *hwdtb_create_proxy(Object *obj, HwDtbProxyFlags flags);

/**
 * hwdtb_is_proxy
 *
 * @return true if the QOM object associated to @node is a proxy object
 */
bool hwdtb_is_proxy(const HwDtbNode *node);

/**
 * hwdtb_proxy_is_to_local
 *
 * @return true if @node is the QOM object associated to @node is a proxy
 * aliasing an object created by the hwdtb subsystem.
 */
bool hwdtb_is_proxy_to_local(const HwDtbNode *node);

/**
 * hwdtb_proxy_is_to_foreign
 *
 * @return true if @node is the QOM object associated to @node is a proxy
 * aliasing an object not created by the hwdtb subsystem.
 */
bool hwdtb_is_proxy_to_foreign(const HwDtbNode *node);

/**
 * hwdtb_proxy_has_flags
 *
 * @return true if @node is the QOM object associated to @node is a proxy
 * and the proxy has the flags @flags set.
 */
bool hwdtb_proxy_has_flags(const HwDtbNode *node, HwDtbProxyFlags flags);

/**
 * hwdtb_get_obj
 *
 * Query the QOM object associated with @node
 *
 * @node the node on which to query the QOM object
 *
 * @return the associated QOM object, or %NULL if the hwdtb creation phase has
 * not been done yet.
 */
Object *hwdtb_get_obj(const HwDtbNode *node);

/**
 * hwdtb_get_parenting_obj
 *
 * Query the QOM object associated with @node. The returned object is meant to
 * be used for parenting other children objects. If the object associated to the
 * node is a proxy, the returning object can either be the proxy itself or the
 * aliased QOM object depending on the proxy flag HWDTB_PROXY_PARENT_ON_OBJ
 * value.
 *
 * @node the node on which to query the QOM object
 *
 * @return the associated QOM object, or %NULL if the hwdtb creation phase has
 * not been done yet.
 */
Object *hwdtb_get_parenting_obj(const HwDtbNode *node);

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
 * hwdtb_node_add_child_obj
 *
 * Add an arbitrary QOM object @child as a child to the QOM object associated
 * with @node. This function takes care of naming the child using the
 * hwdtb-auto<> "namespace" to avoid name clashes in the QOM hierarchy.
 *
 * @node the node for which the QOM object is looked up and serve as the parent
 *       for @child
 * @name the name of the child
 * @child the object to parent
 */
void hwdtb_node_add_child_obj(const HwDtbNode *node, const char *name,
                              Object *child);

/**
 * hwdtb_node_has_gpio_input
 *
 * @return true if the input GPIO (@name, @idx) is found on the qdev associated
 * to @node.
 */
bool hwdtb_node_has_gpio_input(const HwDtbNode *node, const char *name,
                               size_t idx);

/**
 * hwdtb_node_has_gpio_output
 *
 * @return true if the output GPIO (@name, @idx) is found on the qdev associated
 * to @node.
 */
bool hwdtb_node_has_gpio_output(const HwDtbNode *node, const char *name,
                               size_t idx);

/**
 * hwdtb_node_input_visitor_new
 *
 * Create a new input Visitor to parse the FDT properties of @node
 *
 * @return the created visitor
 */
Visitor *hwdtb_node_input_visitor_new(HwDtbNode *node);

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

void hwdtb_node_register_gpio(HwDtbNode *node, const HwDtbResolvedGPIO *gpio,
                              size_t num_conn);

HwDtbRegisteredGPIO *hwdtb_node_get_registered_gpio(HwDtbNode *node,
                                                    const HwDtbResolvedGPIO *gpio);

void hwdtb_str_append_tuple(GString *str, const GArray *tuple);

guint hwdtb_resolved_gpio_hash(gconstpointer a);
gboolean hwdtb_resolved_gpio_equal(gconstpointer a, gconstpointer b);

/**
 * hwdtb_conn_format_get_spec_extended
 *
 * Returns the property name of extended spec for the given connection @kind
 * E.g., interrupts-extended for HWDTB_CON_INTERRUPT
 *
 * This function is used for legacy FDT_GENERIC_GPIO interface support and can
 * be dropped once the former is removed.
 *
 * @return the spec-extended string associated with @kind.
 */
const char *hwdtb_conn_format_get_spec_extended(HwDtbConnectionKind kind);

const char *hwdtb_gpio_get_resolution_str(const HwDtbResolvedGPIO *gpio);
bool hwdtb_gpio_is_resolved(const HwDtbResolvedGPIO *gpio);
#endif
