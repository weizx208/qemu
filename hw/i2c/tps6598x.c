/*
 * TI TPS6598x USB-PD controller - probe stub.
 *
 * Just enough of the host-interface framing for the Linux tipd driver
 * to probe successfully.
 *
 * Copyright Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"
#include "qom/object.h"

#define TYPE_TPS6598X "tps6598x"
OBJECT_DECLARE_SIMPLE_TYPE(TPS6598xState, TPS6598X)

#define TPS_REG_MODE        0x03
#define TPS_MAX_READ_LEN    64

enum tps_phase {
    TPS_IDLE = 0,
    TPS_WR_LEN,
    TPS_WR_DATA,
    TPS_RD_LEN,
    TPS_RD_DATA,
};

struct TPS6598xState {
    I2CSlave parent_obj;

    uint8_t phase;
    uint8_t regptr;
    uint8_t xfer_len;
    uint8_t xfer_pos;
    uint8_t xfer_buf[TPS_MAX_READ_LEN];
};

static void tps_fill_read_buf(TPS6598xState *s)
{
    memset(s->xfer_buf, 0, sizeof(s->xfer_buf));

    if (s->regptr == TPS_REG_MODE) {
        s->xfer_len = 4;
        memcpy(s->xfer_buf, "APP ", 4);
    } else {
        /*
         * Unmodeled register: report a generous payload length so the
         * driver's "data[0] < expected" check never trips. The bytes
         * themselves stay zero.
         */
        s->xfer_len = TPS_MAX_READ_LEN;
    }
}

static int tps6598x_event(I2CSlave *i2c, enum i2c_event event)
{
    TPS6598xState *s = TPS6598X(i2c);

    switch (event) {
    case I2C_START_SEND:
    case I2C_START_SEND_ASYNC:
        s->phase = TPS_IDLE;
        s->xfer_len = 0;
        s->xfer_pos = 0;
        break;
    case I2C_START_RECV:
        s->phase = TPS_RD_LEN;
        s->xfer_pos = 0;
        tps_fill_read_buf(s);
        break;
    case I2C_FINISH:
    case I2C_NACK:
        s->phase = TPS_IDLE;
        s->xfer_pos = 0;
        s->xfer_len = 0;
        break;
    }
    return 0;
}

static int tps6598x_send(I2CSlave *i2c, uint8_t data)
{
    TPS6598xState *s = TPS6598X(i2c);

    switch (s->phase) {
    case TPS_IDLE:
        s->regptr = data;
        s->phase = TPS_WR_LEN;
        return 0;

    case TPS_WR_LEN:
        s->xfer_len = data;
        s->xfer_pos = 0;
        s->phase = TPS_WR_DATA;
        if (s->xfer_len == 0) {
            s->phase = TPS_IDLE;
        }
        return 0;

    case TPS_WR_DATA:
        /* Accept and discard payload bytes. */
        s->xfer_pos++;
        if (s->xfer_pos >= s->xfer_len) {
            s->phase = TPS_IDLE;
        }
        return 0;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "tps6598x: write byte 0x%02x in phase %u\n",
                      data, s->phase);
        return -1;
    }
}

static uint8_t tps6598x_recv(I2CSlave *i2c)
{
    TPS6598xState *s = TPS6598X(i2c);

    switch (s->phase) {
    case TPS_RD_LEN:
        s->phase = TPS_RD_DATA;
        return s->xfer_len;
    case TPS_RD_DATA:
        if (s->xfer_pos < s->xfer_len) {
            return s->xfer_buf[s->xfer_pos++];
        }
        return 0;
    default:
        return 0;
    }
}

static void tps6598x_reset(DeviceState *dev)
{
    TPS6598xState *s = TPS6598X(dev);

    s->phase = TPS_IDLE;
    s->regptr = 0;
    s->xfer_len = 0;
    s->xfer_pos = 0;
    memset(s->xfer_buf, 0, sizeof(s->xfer_buf));
}

static const VMStateDescription vmstate_tps6598x = {
    .name = TYPE_TPS6598X,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_I2C_SLAVE(parent_obj, TPS6598xState),
        VMSTATE_UINT8(phase, TPS6598xState),
        VMSTATE_UINT8(regptr, TPS6598xState),
        VMSTATE_UINT8(xfer_len, TPS6598xState),
        VMSTATE_UINT8(xfer_pos, TPS6598xState),
        VMSTATE_UINT8_ARRAY(xfer_buf, TPS6598xState, TPS_MAX_READ_LEN),
        VMSTATE_END_OF_LIST()
    }
};

static void tps6598x_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass   *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k  = I2C_SLAVE_CLASS(klass);

    k->event = tps6598x_event;
    k->recv  = tps6598x_recv;
    k->send  = tps6598x_send;

    device_class_set_legacy_reset(dc, tps6598x_reset);
    dc->vmsd  = &vmstate_tps6598x;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo tps6598x_info = {
    .name          = TYPE_TPS6598X,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(TPS6598xState),
    .class_init    = tps6598x_class_init,
};

static void tps6598x_register_types(void)
{
    type_register_static(&tps6598x_info);
}

type_init(tps6598x_register_types)
