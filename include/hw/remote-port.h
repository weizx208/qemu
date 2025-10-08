/*
 * QEMU remote port.
 *
 * Copyright (c) 2013 Xilinx Inc
 * Written by Edgar E. Iglesias <edgar.iglesias@xilinx.com>
 *
 * This code is licensed under the GNU GPL.
 */
#ifndef REMOTE_PORT_H__
#define REMOTE_PORT_H__

#include <stdbool.h>
#include "hw/remote-port-proto.h"
#include "hw/remote-port-device.h"
#include "io/channel.h"
#include "hw/ptimer.h"

#define TYPE_REMOTE_PORT "remote-port"
OBJECT_DECLARE_SIMPLE_TYPE(RemotePort, REMOTE_PORT)

typedef struct RemotePortRespSlot {
            RemotePortDynPkt rsp;
            uint32_t id;
            bool used;
            bool valid;
} RemotePortRespSlot;

struct RemotePort {
    DeviceState parent;

    QIOChannel *chan;
    Notifier machine_done;
    bool do_sync;
    bool doing_sync;
    bool finalizing;

    char *chardesc;
    struct rp_peer_state peer;

    struct {
        ptimer_state *ptimer;
        ptimer_state *ptimer_resp;
        bool resp_timer_enabled;
        bool need_sync;
        struct rp_pkt rsp;
        uint64_t quantum;
    } sync;

    QemuCond packet_notify;
    bool receiving; /* a thread is in the middle of a packet reception */

    /*
     * rsp holds responses for the remote side.
     * Used by the slave.
     */
    RemotePortDynPkt rsp;

    /*
     * rspqueue holds received responses from the remote side.
     * Only one for the moment but it might grow.
     * Used by the master.
     */
    RemotePortDynPkt rspqueue;

    char *prefix;

    uint32_t current_id;

#define REMOTE_PORT_MAX_DEVS 1024
#define RP_MAX_OUTSTANDING_TRANSACTIONS 32
    struct {
        RemotePortRespSlot rsp_queue[RP_MAX_OUTSTANDING_TRANSACTIONS];
    } dev_state[REMOTE_PORT_MAX_DEVS];

    RemotePortDevice *devs[REMOTE_PORT_MAX_DEVS];
};

/**
 * rp_device_attach:
 * @adaptor: The adaptor onto which to attach the device
 * @dev: The device to be attached to the adaptor
 * @rp_nr: The remote-port adaptor nr. A device may attach to multiple
 *         adaptors.
 * @dev_nr: The device/channel number to bind the device to.
 * @errp: returns an error if this function fails
 *
 * Attaches a device onto an adaptor and binds it to a device number.
 */
void rp_device_attach(Object *adaptor, Object *dev,
                      int rp_nr, int dev_nr,
                      Error **errp);
void rp_device_detach(Object *adaptor, Object *dev,
                      int rp_nr, int dev_nr,
                      Error **errp);
bool rp_time_warp_enable(bool en);

/**
 * rp_device_add
 * @opts:  qdev opts created by the qdev subsystem
 * @dev: The device to be connected
 * @errp: Returns an error if the function fails
 *
 * Function used in qdev-monitor.c to connect remote port devices.
 * Returns teue on success and false on failure.
 */
bool rp_device_add(const QDict *opts, DeviceState *dev, Error **errp);

static inline void rp_resp_slot_done(RemotePort *s,
                                     RemotePortRespSlot *rsp_slot)
{
    rp_dpkt_invalidate(&rsp_slot->rsp);
    rsp_slot->id = ~0;
    rsp_slot->used = false;
    rsp_slot->valid = false;
}

RemotePortRespSlot *rp_dev_wait_resp(RemotePort *s, uint32_t dev, uint32_t id);

#endif
