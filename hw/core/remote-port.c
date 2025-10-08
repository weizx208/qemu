/*
 * QEMU remote attach
 *
 * Copyright (c) 2013 Xilinx Inc
 * Written by Edgar E. Iglesias <edgar.iglesias@xilinx.com>
 *
 * This code is licensed under the GNU GPL.
 */

#include "qemu/osdep.h"
#include "system/memory.h"
#include "chardev/char.h"
#include "system/cpu-timers.h"
#include "hw/hw.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "exec/icount.h"
#include "hw/core/cpu.h"
#include "io/channel-socket.h"

#include "hw/remote-port-proto.h"
#include "hw/remote-port-device.h"
#include "hw/remote-port.h"

#ifdef RP_DEBUG
#include "qemu/log.h"
#define RP_TRACE(fmt, ...) \
    qemu_log("[%s:%s][%22s] " fmt, g_get_prgname(), \
            current_cpu ? "     vcpu" : qemu_in_coroutine() ? \
                "coroutine" : "main-loop" \
            , __func__, ## __VA_ARGS__)
#else
#define RP_TRACE(fmt, ...) do {} while (0)
#endif

#define RP_TRACE_FUNC() RP_TRACE("\n")

static bool time_warp_enable = true;

bool rp_time_warp_enable(bool en)
{
    bool ret = time_warp_enable;

    time_warp_enable = en;
    return ret;
}

static bool recv_one(RemotePort *s, RemotePortDynPkt *dpkt, bool can_yield);
static void sync_timer_hit(void *opaque);
static void syncresp_timer_hit(void *opaque);

uint32_t rp_new_id(RemotePort *s)
{
    return qatomic_fetch_inc(&s->current_id);
}

void rp_rsp_mutex_lock(RemotePort *s)
{
}

void rp_rsp_mutex_unlock(RemotePort *s)
{
}

int64_t rp_normalized_vmclk(RemotePort *s)
{
    int64_t clk;

    clk = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    clk -= s->peer.clk_base;
    return clk;
}

static void rp_restart_sync_timer_bare(RemotePort *s)
{
    if (!s->do_sync) {
        return;
    }

    if (s->sync.quantum) {
        ptimer_stop(s->sync.ptimer);
        ptimer_set_limit(s->sync.ptimer, s->sync.quantum, 1);
        ptimer_run(s->sync.ptimer, 1);
    }
}

void rp_restart_sync_timer(RemotePort *s)
{
    if (s->doing_sync) {
        return;
    }
    ptimer_transaction_begin(s->sync.ptimer);
    rp_restart_sync_timer_bare(s);
    ptimer_transaction_commit(s->sync.ptimer);
}

static void rp_exit(RemotePort *s, const char *reason, int code)
{
    int64_t clk = rp_normalized_vmclk(s);
    error_report("%s: %s clk=%" PRIu64 " ns\n", s->prefix, reason, clk);
    exit(code);
}

static void rp_fatal_error(RemotePort *s, const char *reason)
{
    rp_exit(s, reason, EXIT_FAILURE);
}

ssize_t rp_write(RemotePort *s, const void *buf, size_t count)
{
    ssize_t r;
#ifdef RP_DEBUG
    const struct rp_pkt *pkt = (const struct rp_pkt *) buf;
#endif

    RP_TRACE("sending: %s, id: %u, dev: %u\n",
             rp_cmd_to_string(be32_to_cpu(pkt->hdr.cmd)),
             be32_to_cpu(pkt->hdr.id), be32_to_cpu(pkt->hdr.dev));

    r = qio_channel_write_all(s->chan, buf, count, &error_fatal);

    return r;
}

/* Response handling.  */
RemotePortRespSlot *rp_dev_wait_resp(RemotePort *s, uint32_t dev, uint32_t id)
{
    int i;
    RemotePortDynPkt dpkt = {};

    assert(s->devs[dev]);

    /* Find a free slot.  */
    for (i = 0; i < ARRAY_SIZE(s->dev_state[dev].rsp_queue); i++) {
        if (s->dev_state[dev].rsp_queue[i].used == false) {
            break;
        }
    }

    if (i == ARRAY_SIZE(s->dev_state[dev].rsp_queue) ||
        s->dev_state[dev].rsp_queue[i].used == true) {
        error_report("Number of outstanding transactions exceeded! %d",
                      RP_MAX_OUTSTANDING_TRANSACTIONS);
        rp_fatal_error(s, "Internal error");
    }

    /* Got a slot, fill it in.  */
    s->dev_state[dev].rsp_queue[i].id = id;
    s->dev_state[dev].rsp_queue[i].valid = false;
    s->dev_state[dev].rsp_queue[i].used = true;

    rp_dpkt_alloc(&dpkt, sizeof(dpkt.pkt->busaccess) + 1024);

    while (!s->dev_state[dev].rsp_queue[i].valid) {
        RP_TRACE("dev %u wait for response id %u\n", dev, id);
        if (!recv_one(s, &dpkt, false)) {
            rp_exit(s, "Disconnected", 0);
        }
    }

    rp_dpkt_free(&dpkt);
    return &s->dev_state[dev].rsp_queue[i];
}

RemotePortDynPkt rp_wait_resp(RemotePort *s)
{
    RemotePortDynPkt dpkt = {};

    rp_dpkt_alloc(&dpkt, sizeof(dpkt.pkt->busaccess) + 1024);

    while (!rp_dpkt_is_valid(&s->rspqueue)) {
        RP_TRACE("%s: wait for progress\n", __func__);
        if (!recv_one(s, &dpkt, true)) {
            rp_exit(s, "Disconnected", 0);
        }
    }

    rp_dpkt_free(&dpkt);
    return s->rspqueue;
}

static void rp_cmd_hello(RemotePort *s, struct rp_pkt *pkt)
{
    s->peer.version = pkt->hello.version;
    if (pkt->hello.version.major != RP_VERSION_MAJOR) {
        error_report("remote-port version missmatch remote=%d.%d local=%d.%d\n",
                      pkt->hello.version.major, pkt->hello.version.minor,
                      RP_VERSION_MAJOR, RP_VERSION_MINOR);
        rp_fatal_error(s, "Bad version");
    }

    if (pkt->hello.caps.len) {
        void *caps = (char *) pkt + pkt->hello.caps.offset;

        rp_process_caps(&s->peer, caps, pkt->hello.caps.len);
    }
}

static void rp_cmd_sync(RemotePort *s, struct rp_pkt *pkt)
{
    size_t enclen;
    int64_t clk;
    int64_t diff;

    assert(!(pkt->hdr.flags & RP_PKT_FLAGS_response));

    clk = rp_normalized_vmclk(s);
    diff = pkt->sync.timestamp - clk;

    enclen = rp_encode_sync_resp(pkt->hdr.id, pkt->hdr.dev, &s->sync.rsp.sync,
                                 pkt->sync.timestamp);
    assert(enclen == sizeof s->sync.rsp.sync);

    /* We have temporarily disabled blocking syncs into QEMU.  */
    if (diff <= 0LL || true) {
        /* We are already a head of time. Respond and issue a sync.  */
        RP_TRACE("%s: sync resp %lu\n", s->prefix, pkt->sync.timestamp);
        rp_write(s, (void *) &s->sync.rsp, enclen);
        return;
    }

    RP_TRACE("%s: delayed sync resp - start diff=%ld (ts=%lu clk=%lu)\n",
             s->prefix, pkt->sync.timestamp - clk, pkt->sync.timestamp, clk);

    ptimer_transaction_begin(s->sync.ptimer_resp);
    ptimer_set_limit(s->sync.ptimer_resp, diff, 1);
    ptimer_run(s->sync.ptimer_resp, 1);
    s->sync.resp_timer_enabled = true;
    ptimer_transaction_commit(s->sync.ptimer_resp);
}

static void rp_say_hello(RemotePort *s)
{
    struct rp_pkt_hello pkt;
    uint32_t caps[] = {
        CAP_BUSACCESS_EXT_BASE,
        CAP_BUSACCESS_EXT_BYTE_EN,
        CAP_WIRE_POSTED_UPDATES,
        CAP_ATS,
    };
    size_t len;

    len = rp_encode_hello_caps(s->current_id++, 0, &pkt, RP_VERSION_MAJOR,
                               RP_VERSION_MINOR,
                               caps, caps, sizeof caps / sizeof caps[0]);
    rp_write(s, (void *) &pkt, len);

    if (sizeof caps) {
        rp_write(s, caps, sizeof caps);
    }
}

static void rp_say_sync(RemotePort *s, int64_t clk)
{
    struct rp_pkt_sync pkt;
    size_t len;

    len = rp_encode_sync(s->current_id++, 0, &pkt, clk);
    rp_write(s, (void *) &pkt, len);
}

static void syncresp_timer_hit(void *opaque)
{
    RemotePort *s = REMOTE_PORT(opaque);

    s->sync.resp_timer_enabled = false;
    RP_TRACE("%s: delayed sync response - send\n", s->prefix);
    rp_write(s, (void *) &s->sync.rsp, sizeof s->sync.rsp.sync);
    memset(&s->sync.rsp, 0, sizeof s->sync.rsp);
}

static void sync_timer_hit(void *opaque)
{
    RemotePort *s = REMOTE_PORT(opaque);
    int64_t clk;
    RemotePortDynPkt rsp;

    clk = rp_normalized_vmclk(s);
    if (s->sync.resp_timer_enabled) {
        RP_TRACE("%s: sync while delaying a resp! clk=%lu\n", s->prefix, clk);
        s->sync.need_sync = true;
        rp_restart_sync_timer_bare(s);
        return;
    }

    /* Sync.  */
    s->doing_sync = true;
    s->sync.need_sync = false;
    /* Send the sync.  */
    rp_say_sync(s, clk);

    RP_TRACE("%s: syncing wait for resp %lu\n", s->prefix, clk);
    rsp = rp_wait_resp(s);
    rp_dpkt_invalidate(&rsp);
    s->doing_sync = false;

    rp_restart_sync_timer_bare(s);
}

static char *rp_sanitize_prefix(RemotePort *s)
{
    char *sanitized_name;
    char *c;

    sanitized_name = g_strdup(s->prefix);
    for (c = sanitized_name; *c != '\0'; c++) {
        if (*c == '/')
            *c = '_';
    }
    return sanitized_name;
}

static QIOChannel *rp_create_iochannel(SocketAddress *addr, bool server,
                                       Error **errp)
{
    QIOChannelSocket *sock = qio_channel_socket_new();

    if (server) {
        QIOChannelSocket *client;

        if (qio_channel_socket_listen_sync(sock, addr, 1, errp)) {
            object_unref(OBJECT(sock));
            return NULL;
        }

        client = qio_channel_socket_accept(sock, errp);
        object_unref(OBJECT(sock));

        return QIO_CHANNEL(client);
    } else {
        if (qio_channel_socket_connect_sync(sock, addr, errp)) {
            object_unref(OBJECT(sock));
            return NULL;
        }

        return QIO_CHANNEL(sock);
    }
}

/*
 * The remote port code used to have a Chardev instead of an QIOChannel for
 * communication with the peer. The remote-port device has this `chardesc'
 * string qdev property that was passed to the Chardev. To maintain backward
 * compatibility, this function parses it and set the SocketAddress accordingly
 * for the QIOChannelSocket creation. Only TCP address/port and UNIX socket are
 * supported.
 */
static bool rp_parse_legacy_chardesc(RemotePort *s, SocketAddress *addr,
                                     bool *server, Error **errp)
{
    QemuOpts *opts;
    const char *backend;
    const char *path;
    const char *host;
    const char *port;
    const char *fd;

    opts = qemu_chr_parse_compat("remote-port", s->chardesc, false);
    if (opts == NULL) {
        error_setg(errp, "Error while parsing the chardesc property");
        return false;
    }

    backend = qemu_opt_get(opts, "backend");

    if (backend == NULL) {
        error_setg(errp, "remote-port chardesc property: missing backend");
        return false;
    }

    if (strcmp(backend, "socket")) {
        error_setg(errp, "remote-port chardesc property: "
                   "only the socket backend is supported");
        error_append_hint(errp, "only use tcp: or unix: "
                          "in the chardesc property");
        return false;
    }

    path = qemu_opt_get(opts, "path");
    host = qemu_opt_get(opts, "host");
    port = qemu_opt_get(opts, "port");
    fd = qemu_opt_get(opts, "fd");

    if ((!!path + !!fd + !!host) > 1) {
        error_setg(errp, "remote-port chardesc property: "
                   "path, host and fd are mutually exclusive");
        return false;
    }

    if (host && !port) {
        error_setg(errp, "remote-port chardesc property: missing port");
        return false;
    }

    if (path) {
        addr->type = SOCKET_ADDRESS_TYPE_UNIX;
        addr->u.q_unix.path = g_strdup(path);
#ifdef CONFIG_LINUX
        addr->u.q_unix.has_tight = true;
        addr->u.q_unix.tight = qemu_opt_get_bool(opts, "tight", true);
        addr->u.q_unix.has_abstract = true;
        addr->u.q_unix.abstract = qemu_opt_get_bool(opts, "abstract", false);
#endif
    } else if (host) {
        addr->type = SOCKET_ADDRESS_TYPE_INET;
        addr->u.inet.host = g_strdup(host);
        addr->u.inet.port = g_strdup(port);
        addr->u.inet.has_to = qemu_opt_get(opts, "to");
        addr->u.inet.to = qemu_opt_get_number(opts, "to", 0);
        addr->u.inet.has_ipv4 = qemu_opt_get(opts, "ipv4");
        addr->u.inet.ipv4 = qemu_opt_get_bool(opts, "ipv4", 0);
        addr->u.inet.has_ipv6 = qemu_opt_get(opts, "ipv6");
        addr->u.inet.ipv6 = qemu_opt_get_bool(opts, "ipv6", 0);
    } else {
        addr->type = SOCKET_ADDRESS_TYPE_FD;
        addr->u.fd.str = g_strdup(fd);
    }

    *server = qemu_opt_get_bool(opts, "server", false);

    qemu_opts_del(opts);

    return true;
}

/*
 * Legacy message from when a chardev was used. Some scripts rely on it so
 * keep it for backward compatibility.
 */
static void rp_report_listening(const SocketAddress *addr, const char *prefix)
{
    switch (addr->type) {
    case SOCKET_ADDRESS_TYPE_UNIX:
        if (prefix) {
            info_report("QEMU waiting for connection on: %s/%s",
                        prefix, addr->u.q_unix.path);
        } else {
            info_report("QEMU waiting for connection on: %s",
                        addr->u.q_unix.path);
        }
        break;

    case SOCKET_ADDRESS_TYPE_INET:
        g_assert(prefix == NULL);

        info_report("QEMU waiting for connection on: tcp:%s:%s",
                    addr->u.inet.host, addr->u.inet.port);
    default:
        break;
    }

}

static QIOChannel *rp_connect_legacy_chardesc(RemotePort *s, Error **errp)
{
    SocketAddress addr = {};
    bool server;

    if (!rp_parse_legacy_chardesc(s, &addr, &server, errp)) {
        return NULL;
    }

    if (server) {
        rp_report_listening(&addr, NULL);
    }

    return rp_create_iochannel(&addr, server, errp);
}

static QIOChannel *rp_autoconnect(RemotePort *s, Error **errp)
{
    SocketAddress addr = {};
    g_autofree char *socket_path = NULL;
    g_autofree char *saved_cwd = NULL;
    QIOChannel *ret = NULL;
    bool socket_exists;

    if (!machine_path) {
        error_setg(errp, "%s: Missing chardesc prop. Forgot -machine-path?",
                   s->prefix);
        return NULL;
    }

    saved_cwd = g_get_current_dir();

    /*
     * To workaround AF_UNIX socket path limitation, this function chdir to the
     * machine-path directory, and put a relative socket file name into the
     * socket path.
     */
    if (chdir(machine_path)) {
        error_setg(errp, "Cannot chdir to machine path `%s'", machine_path);
        return NULL;
    }

    socket_path = g_strdup_printf("qemu-rport-%s", rp_sanitize_prefix(s));
    socket_exists = g_file_test(socket_path, G_FILE_TEST_EXISTS);

    addr.type = SOCKET_ADDRESS_TYPE_UNIX;
    addr.u.q_unix.abstract = false;
    addr.u.q_unix.tight = false;
    addr.u.q_unix.path = socket_path;

    if (socket_exists) {
        ret = rp_create_iochannel(&addr, false, NULL);
    }

    if (ret == NULL) {
        rp_report_listening(&addr, machine_path);
        ret = rp_create_iochannel(&addr, true, errp);
    }

    if (chdir(saved_cwd)) {
        error_setg(errp, "Cannot chdir back to `%s'", saved_cwd);
        object_unref(OBJECT(ret));

        return NULL;
    }

    return ret;
}

static void rp_process(RemotePort *s, RemotePortDynPkt *dpkt)
{
    struct rp_pkt *pkt;
    bool actioned = false;
    RemotePortDevice *dev;
    RemotePortDeviceClass *rpdc;

    g_assert(bql_locked());

    pkt = dpkt->pkt;
    dev = s->devs[pkt->hdr.dev];
    if (dev) {
        rpdc = REMOTE_PORT_DEVICE_GET_CLASS(dev);
        if (rpdc->ops[pkt->hdr.cmd]) {
            RP_TRACE("forward packet to dev %s\n",
                     object_get_canonical_path_component(OBJECT(dev)));
            rpdc->ops[pkt->hdr.cmd](dev, pkt);
            actioned = true;
        }
    }

    switch (pkt->hdr.cmd) {
    case RP_CMD_sync:
        rp_cmd_sync(s, pkt);
        break;
    default:
        assert(actioned);
    }
}

static bool rp_pt_cmd_sync(RemotePort *s, struct rp_pkt *pkt)
{
    size_t enclen;
    int64_t clk;
    int64_t diff = 0;
    struct rp_pkt rsp;

    assert(!(pkt->hdr.flags & RP_PKT_FLAGS_response));

    if (use_icount) {
        clk = rp_normalized_vmclk(s);
        diff = pkt->sync.timestamp - clk;
    }
    enclen = rp_encode_sync_resp(pkt->hdr.id, pkt->hdr.dev, &rsp.sync,
                                 pkt->sync.timestamp);
    assert(enclen == sizeof rsp.sync);

    if (!use_icount || diff < s->sync.quantum) {
        /* We are still OK.  */
        rp_write(s, (void *) &rsp, enclen);
        return true;
    }

    /* We need IO or CPU thread sync.  */
    return false;
}

static bool rp_pt_process_pkt(RemotePort *s, RemotePortDynPkt *dpkt)
{
    struct rp_pkt *pkt = dpkt->pkt;

    if (pkt->hdr.dev >= ARRAY_SIZE(s->devs)) {
        /* FIXME: Respond with an error.  */
        return true;
    }

    if (pkt->hdr.flags & RP_PKT_FLAGS_response) {
        uint32_t dev = pkt->hdr.dev;
        uint32_t id = pkt->hdr.id;
        int i;

        RP_TRACE("received response: %s, id: %u, dev: %u\n",
                 rp_cmd_to_string(pkt->hdr.cmd), pkt->hdr.id, pkt->hdr.dev);

        if (pkt->hdr.flags & RP_PKT_FLAGS_posted) {
            RP_TRACE("Drop response for posted packets\n");
            return true;
        }

        /* Try to find a per-device slot first.  */
        for (i = 0; i < ARRAY_SIZE(s->dev_state[dev].rsp_queue); i++) {
            if (s->devs[dev] && s->dev_state[dev].rsp_queue[i].used == true
                && s->dev_state[dev].rsp_queue[i].id == id) {
                break;
            }
        }

        if (i < ARRAY_SIZE(s->dev_state[dev].rsp_queue)) {
            /* Found a per device one.  */
            assert(s->dev_state[dev].rsp_queue[i].valid == false);

            rp_dpkt_swap(&s->dev_state[dev].rsp_queue[i].rsp, dpkt);
            s->dev_state[dev].rsp_queue[i].valid = true;
        } else {
            rp_dpkt_swap(&s->rspqueue, dpkt);
        }

        return true;
    }

    RP_TRACE("received: %s, id: %u, dev: %u\n",
             rp_cmd_to_string(pkt->hdr.cmd), pkt->hdr.id, pkt->hdr.dev);

    switch (pkt->hdr.cmd) {
    case RP_CMD_hello:
        rp_cmd_hello(s, pkt);
        break;
    case RP_CMD_sync:
        if (rp_pt_cmd_sync(s, pkt)) {
            return true;
        }
        /* Fall-through.  */
    case RP_CMD_read:
    case RP_CMD_write:
    case RP_CMD_interrupt:
    case RP_CMD_ats_req:
    case RP_CMD_ats_inv:
        rp_process(s, dpkt);
        break;
    default:
        g_assert_not_reached();
    }
    return false;
}

/*
 * rp_recv: receive data from the iochannel.
 *
 * Receive count bytes from the iochannel, or possibly nothing if blocking is
 * false.
 *
 * @return the number of read bytes equal to count, or QIO_CHANNEL_ERR_BLOCK if
 * blocking was false and no data were available on the iochannel.
 */
static ssize_t coroutine_mixed_fn rp_recv(RemotePort *s, void *buf,
                                          size_t count, bool blocking,
                                          Error **errp)
{
    ssize_t total = 0, ret;

    if (!blocking) {
        total = qio_channel_read(s->chan, buf, count, errp);

        if (total < 0) {
            return total;
        }
    }

    ret = qio_channel_read_all(s->chan, buf + total, count - total, errp);

    if (ret < 0) {
        return ret;
    }

    total = count;

    RP_TRACE("%zd\n", total);
    return total;
}

/*
 * rp_read_pkt: read a complete packet, or nothing. This function is
 * non-blocking if no data are available on the iochannel when called. If at
 * least one byte of data is available, it becomes blocking and guarantee to
 * read the complete packet unless an error happens. s->receiving is set to true
 * while in this function.
 *
 * @return the number of read bytes, or QIO_CHANNEL_ERR_BLOCK if no packets were
 * available and nothing has been read.
 */
static coroutine_mixed_fn int rp_read_pkt(RemotePort *s, RemotePortDynPkt *dpkt,
                                          Error **errp)
{
    struct rp_pkt *pkt = dpkt->pkt;
    ssize_t hdr_len = 0;
    ssize_t payload_len = 0;

    g_assert(!s->receiving);
    s->receiving = true;

    hdr_len = rp_recv(s, pkt, sizeof(pkt->hdr), false, errp);
    if (hdr_len <= 0) {
        goto out;
    }

    rp_decode_hdr(pkt);

    if (pkt->hdr.len) {
        rp_dpkt_alloc(dpkt, sizeof(pkt->hdr) + pkt->hdr.len);
        /* pkt may move due to realloc.  */
        pkt = dpkt->pkt;
        payload_len = rp_recv(s, &pkt->hdr + 1, pkt->hdr.len, true, errp);

        if (payload_len <= 0) {
            hdr_len = 0;
            goto out;
        }
        rp_decode_payload(pkt);
    }

out:
    s->receiving = false;
    return hdr_len + payload_len;
}

/*
 * Wait for data to arrive on the iochannel. Given the caller context and the
 * remote port state, this function will wait on various sources:
 *    - when called from the remote-port receiving coroutine, it will yield
 *      using qio_channel_yield() if can_yield is true, waiting for data on the
 *      channel. can_yield is true when the coroutine is simply waiting for new
 *      packets to arrive and false when it is actually waiting for an inline
 *      response from a request it made.
 *    - when called from a vCPU thread, it will:
 *       * try to take the reception lead if no packet are currently being
 *         received by another thread,
 *       * wait on s->packet_notify otherwise.
 *    - when called from the main thread, simply call qio_channel_wait to wait
 *      for the inline response of the request if yielding is not permitted.
 *      Otherwise run the main loop.
 *
 * This "vCPU taking the lead" logic is an optimization for the common case
 * where a vCPU sends a packet and synchronously waits for the response. It
 * avoids costly thread synchronisation between the main thread and the vCPU
 * thread.
 */
static coroutine_mixed_fn void wait_for_packet(RemotePort *s, bool can_yield)
{
    if (qemu_in_coroutine()) {
        if (can_yield) {
            qio_channel_yield(s->chan, G_IO_IN);
        } else {
            qio_channel_wait(s->chan, G_IO_IN);
        }
    } else if (current_cpu) {
        if (s->receiving) {
            /*
             * Cannot take the reception lead here in packet reception, because
             * someone is already in the middle of a packet. Wait on the costly
             * packet_notify condition.
             */
            qemu_cond_wait_bql(&s->packet_notify);
        } else {
            /*
             * Take the packet reception lead. Keep the BQL to avoid concurrent
             * waits on the channel.
             */
            qio_channel_wait(s->chan, G_IO_IN);
        }
    } else {
        /*
         * On the main loop thread, outside the co-routine. Call the main loop
         * or wait on the QIOChannel.
         */
        if (can_yield) {
            main_loop_wait(false);
        } else {
            qio_channel_wait(s->chan, G_IO_IN);
        }
    }
}

/*
 * Notify a new packet has been received. This is the counterpart of
 * wait_for_packet().
 */
static coroutine_mixed_fn void notify_packet(RemotePort *s)
{
    /*
     * Unconditionally notify the packet_notify condition:
     *   - If we are in the coroutine or outside on the main thread, we want to
     *     notify the vCPU threads of the packet.
     *   - If we are on a vCPU thread, other vCPU threads thread might be
     *     waiting for a packet.
     */
    qemu_cond_broadcast(&s->packet_notify);

    if (!qemu_in_coroutine()) {
        if (current_cpu) {
            /*
             * On a vCPU thread, unlock the BQL before calling
             * qio_channel_wake_read. This ensures the coroutine won't be
             * entered on a vCPU thread. This is necessary because the coroutine
             * might have yielded in the middle of a memory region access
             * waiting for an response from the peer. In that case the RCU
             * thread will assert because the RCU lock/unlock pair in the memory
             * subsystem won't happen on the same threads.
             */
            bql_unlock();
        }

        qio_channel_wake_read(s->chan);

        if (current_cpu) {
            bql_lock();
        }
    }
}

/*
 * recv_one: receive and process one packet.
 *
 * @return: true after the packet has been received and processed, false if a
 * iochannel read call failed (e.g. because the connection was dropped).
 */
static coroutine_mixed_fn bool recv_one(RemotePort *s, RemotePortDynPkt *dpkt,
                                        bool can_yield)
{
    ssize_t r;

    RP_TRACE_FUNC();

    if (s->receiving) {
        /*
         * A thread is already in the middle of a packet reception. Wait for it
         * to finish.
         */
        wait_for_packet(s, can_yield);
        return true;
    }

    r = rp_read_pkt(s, dpkt, NULL);

    if (r == QIO_CHANNEL_ERR_BLOCK) {
        /* No packet were available. Wait */
        wait_for_packet(s, can_yield);
        return true;
    }

    if (r <= 0) {
        /* Error while reading the iochannel */
        return false;
    }

    rp_pt_process_pkt(s, dpkt);
    notify_packet(s);

    return true;
}

static coroutine_fn void rp_recv_coroutine(void *arg)
{
    RemotePort *s = REMOTE_PORT(arg);
    RemotePortDynPkt dpkt = {};
    bool ok;

    RP_TRACE_FUNC();

    /* Make sure we have a decent bufsize to start with.  */
    rp_dpkt_alloc(&s->rsp, sizeof s->rsp.pkt->busaccess + 1024);
    rp_dpkt_alloc(&s->rspqueue, sizeof s->rspqueue.pkt->busaccess + 1024);
    rp_dpkt_alloc(&dpkt, sizeof(dpkt.pkt->busaccess) + 1024);

    rp_say_hello(s);

    do {
        ok = recv_one(s, &dpkt, true);
    } while (ok);

    if (!s->finalizing) {
        rp_exit(s, "Disconnected", 0);
    }
}

static void rp_machine_done(Notifier *notifier, void *data)
{
    Coroutine *co;
    RemotePort *s = container_of(notifier, RemotePort, machine_done);

    RP_TRACE_FUNC();

    main_loop_poll_remove_notifier(&s->machine_done);

    co = qemu_coroutine_create(rp_recv_coroutine, s);
    qemu_coroutine_enter(co);

    rp_restart_sync_timer(s);
}

static void rp_realize(DeviceState *dev, Error **errp)
{
    RemotePort *s = REMOTE_PORT(dev);

    if (s->prefix == NULL) {
        s->prefix = object_get_canonical_path(OBJECT(dev));
    }

    s->peer.clk_base = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    qemu_cond_init(&s->packet_notify);

    if (s->chardesc) {
        s->chan = rp_connect_legacy_chardesc(s, errp);
    } else {
        s->chan = rp_autoconnect(s, errp);
    }

    if (s->chan == NULL) {
        return;
    }

    /*
     * The QIOChannel is set non-blocking to allow the coroutine to yield when
     * waiting for a packet.
     */
    qio_channel_set_blocking(s->chan, false, NULL);

    /* Pick up the quantum from the local property setup.
       After config negotiation with the peer, sync.quantum value might
       change.  */
    s->sync.quantum = s->peer.local_cfg.quantum;

    s->sync.ptimer = ptimer_init(sync_timer_hit, s, PTIMER_POLICY_LEGACY);
    s->sync.ptimer_resp = ptimer_init(syncresp_timer_hit, s,
                                      PTIMER_POLICY_LEGACY);

    /* The Sync-quantum is expressed in nano-seconds.  */
    ptimer_transaction_begin(s->sync.ptimer);
    ptimer_set_freq(s->sync.ptimer, 1000 * 1000 * 1000);
    ptimer_transaction_commit(s->sync.ptimer);

    ptimer_transaction_begin(s->sync.ptimer_resp);
    ptimer_set_freq(s->sync.ptimer_resp, 1000 * 1000 * 1000);
    ptimer_transaction_commit(s->sync.ptimer_resp);
}

static void rp_unrealize(DeviceState *dev)
{
    RemotePort *s = REMOTE_PORT(dev);

    s->finalizing = true;

    info_report("%s: Wait for remote-port to disconnect\n", s->prefix);
    object_unref(OBJECT(s->chan));
}

static const VMStateDescription vmstate_rp = {
    .name = TYPE_REMOTE_PORT,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST(),
    }
};

static const Property rp_properties[] = {
    DEFINE_PROP_STRING("chardesc", RemotePort, chardesc),
    DEFINE_PROP_BOOL("sync", RemotePort, do_sync, false),
    DEFINE_PROP_UINT64("sync-quantum", RemotePort, peer.local_cfg.quantum,
                       1000000),
    DEFINE_PROP_STRING("prefix", RemotePort, prefix),
};

static void rp_init(Object *obj)
{
    RemotePort *s = REMOTE_PORT(obj);
    int t;
    int i;

    for (i = 0; i < REMOTE_PORT_MAX_DEVS; ++i) {
        char *name = g_strdup_printf("remote-port-dev%d", i);
        object_property_add_link(obj, name, TYPE_REMOTE_PORT_DEVICE,
                             (Object **)&s->devs[i],
                             qdev_prop_allow_set_link,
                             OBJ_PROP_LINK_STRONG);
        g_free(name);


        for (t = 0; t < RP_MAX_OUTSTANDING_TRANSACTIONS; t++) {
            s->dev_state[i].rsp_queue[t].used = false;
            s->dev_state[i].rsp_queue[t].valid = false;
            rp_dpkt_alloc(&s->dev_state[i].rsp_queue[t].rsp,
               sizeof s->dev_state[i].rsp_queue[t].rsp.pkt->busaccess + 1024);
        }
    }

    s->machine_done.notify = rp_machine_done;
    main_loop_poll_add_notifier(&s->machine_done);
}

static void rp_finalize(Object *obj)
{
    RemotePort *s = REMOTE_PORT(obj);

    if (s->machine_done.node.le_next != NULL) {
        main_loop_poll_remove_notifier(&s->machine_done);
    }
}

struct rp_peer_state *rp_get_peer(RemotePort *s)
{
    return &s->peer;
}

static void rp_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = rp_realize;
    dc->unrealize = rp_unrealize;
    dc->vmsd = &vmstate_rp;
    device_class_set_props(dc, rp_properties);
}

static const TypeInfo rp_info = {
    .name          = TYPE_REMOTE_PORT,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(RemotePort),
    .instance_init = rp_init,
    .instance_finalize = rp_finalize,
    .class_init    = rp_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { },
    },
};

static const TypeInfo rp_device_info = {
    .name          = TYPE_REMOTE_PORT_DEVICE,
    .parent        = TYPE_INTERFACE,
    .class_size    = sizeof(RemotePortDeviceClass),
};

static void rp_register_types(void)
{
    type_register_static(&rp_info);
    type_register_static(&rp_device_info);
}

type_init(rp_register_types)
