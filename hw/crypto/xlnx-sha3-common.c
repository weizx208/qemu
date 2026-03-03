/*
 * Common base model for AMD / Xilinx SHA3 IPs.
 *
 * Copyright (c) 2023, Advanced Micro Devices, Inc.
 * Developed by Fred Konrad <fkonrad@amd.com>
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
#include "hw/sysbus.h"
#include "hw/register.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/hw.h"
#include "qapi/error.h"
#include "hw/crypto/xlnx-sha3-common.h"

#ifndef XLNX_SHA3_COMMON_ERR_DEBUG
#define XLNX_SHA3_COMMON_ERR_DEBUG 0
#endif

/* Describe the current state of the crypto model.  */
enum State {
    XLNX_SHA3_COMMON_IDLE = 0,
    XLNX_SHA3_COMMON_RESETING,
    XLNX_SHA3_COMMON_RUNNING,
};

#define XLNX_SHA3_COMMON_MAX_DIGEST_LEN (1344 >> 3)

static bool xlnx_sha3_common_autopadding_enabled(XlnxSha3Common *s)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->is_autopadding_enabled);
    return k->is_autopadding_enabled(s);
}

static void xlnx_sha3_common_handle_end_of_packet(XlnxSha3Common *s)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->end_of_packet_notifier);
    k->end_of_packet_notifier(s);
}

static void xlnx_sha3_common_write_digest(XlnxSha3Common *s,
                                          uint32_t *digest,
                                          size_t size)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->write_digest);
    k->write_digest(s, digest, size);
}

static XlnxSha3CommonAlg xlnx_sha3_common_get_algorithm(XlnxSha3Common *s)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->get_algorithm);
    return k->get_algorithm(s);
}

static uint8_t xlnx_sha3_common_get_chain_start(XlnxSha3Common *s)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->get_chain_start);
    return k->get_chain_start(s);
}

static uint8_t xlnx_sha3_common_get_chain_steps(XlnxSha3Common *s)
{
    XlnxSha3CommonClass *k = XLNX_SHA3_COMMON_GET_CLASS(s);

    assert(k->get_chain_steps);
    return k->get_chain_steps(s);
}

static size_t xlnx_sha3_common_block_size(XlnxSha3Common *s)
{
    switch (s->alg) {
    case SHA_MODE_SHAKE256_LMS_OTS_CHAIN:
    case SHA_MODE_SHAKE256_SLH_DSA_CHAIN:
    case SHA_MODE_256:
    case SHA_MODE_SHAKE256:
    case SHA_MODE_SHAKE256_256:
        return 136;
    case SHA_MODE_SHAKE128:
        return 168;
    case SHA_MODE_384:
        return 104;
    case SHA_MODE_512:
        return 72;
    default:
        return 0;
    }
}

static size_t xlnx_sha3_common_digest_size(XlnxSha3Common *s)
{
    switch (s->alg) {
    case SHA_MODE_256:
    case SHA_MODE_SHAKE256_256:
        return 256 / 8;
    case SHA_MODE_384:
        return 384 / 8;
    case SHA_MODE_512:
        return 512 / 8;
    case SHA_MODE_SHAKE256_LMS_OTS_CHAIN:
    case SHA_MODE_SHAKE256_SLH_DSA_CHAIN:
    case SHA_MODE_SHAKE256:
        return 1088 / 8;
    case SHA_MODE_SHAKE128:
        return 1344 / 8;
    default:
        return 0;
    }
}

/* Returns padding suffix as described in FIPS202.  */
static uint8_t xlnx_sha3_common_get_padding_suffix(XlnxSha3Common *s)
{
    switch (s->alg) {
    case SHA_MODE_SHAKE128:
    case SHA_MODE_SHAKE256_LMS_OTS_CHAIN:
    case SHA_MODE_SHAKE256_SLH_DSA_CHAIN:
    case SHA_MODE_SHAKE256:
    case SHA_MODE_SHAKE256_256:
        return 0x1f;
    default:
        return 0x06;
    }
}

static void xlnx_sha3_common_log_guest_error(XlnxSha3Common *s,
                                     const char *fn,
                                     const char *msg)
{
    char *path = object_get_canonical_path(OBJECT(s));

    qemu_log_mask(LOG_GUEST_ERROR, "%s: %s: %s", path, fn, msg);
    g_free(path);
}

void xlnx_sha3_common_start(XlnxSha3Common *s)
{
    if (s->state == XLNX_SHA3_COMMON_RESETING) {
        return;
    }

    /* Latch the current algorithm.  */
    s->alg = xlnx_sha3_common_get_algorithm(s);

    switch (s->alg) {
    case SHA_MODE_256:
    case SHA_MODE_384:
    case SHA_MODE_512:
    case SHA_MODE_SHAKE128:
    case SHA_MODE_SHAKE256:
    case SHA_MODE_SHAKE256_256:
    case SHA_MODE_SHAKE256_LMS_OTS_CHAIN:
    case SHA_MODE_SHAKE256_SLH_DSA_CHAIN:
        break;
    default:
        /*
         * Unsupported bit-field, throw a guest error, and don't put the
         * model in RUNNING mode (it won't accept data from the DMA).
         */
        xlnx_sha3_common_log_guest_error(s, __func__,
                                         "unsupported SHA3 algorithm\n");
        return;
    }

    /* Initialize the keccak sponge.  */
    keccak_init(&s->sponge);
    /* All is ok.  Indicate the streaming device we can accept data.  */
    s->state = XLNX_SHA3_COMMON_RUNNING;
    s->data_ptr = 0;
}

void xlnx_sha3_common_reset(XlnxSha3Common *s, int reseting)
{
    if (reseting) {
        /* Puts the device in reset mode.  */
        s->state = XLNX_SHA3_COMMON_RESETING;
    } else if (s->state == XLNX_SHA3_COMMON_RESETING) {
        /* 1 -> 0 release from reset mode.  */
        s->state = XLNX_SHA3_COMMON_IDLE;
        s->data_ptr = 0;
    }
}

void xlnx_sha3_common_next_xof(XlnxSha3Common *s)
{
    /* User asks 136 additional SHAKE256 digest.  */
    if (!(s->alg == SHA_MODE_SHAKE128 ||
          s->alg == SHA_MODE_SHAKE256 ||
          s->alg == SHA_MODE_SHAKE256_LMS_OTS_CHAIN ||
          s->alg == SHA_MODE_SHAKE256_SLH_DSA_CHAIN)) {
        xlnx_sha3_common_log_guest_error(s, __func__,
                                         "IP expected to be in SHAKE128/SHAKE256"
                                         " mode\n");
        return;
    }

    /* It is expected that the digest has been computed.  */
    keccak_permute(&s->sponge);
}

static bool xlnx_sha3_common_stream_can_push(StreamSink *obj,
                                             StreamCanPushNotifyFn notify,
                                             void *notify_opaque)
{
    XlnxSha3Common *s = XLNX_SHA3_COMMON(obj);

    return s->state == XLNX_SHA3_COMMON_RUNNING;
}

void xlnx_sha3_common_update_digest(XlnxSha3Common *s)
{
    uint32_t digest[XLNX_SHA3_COMMON_MAX_DIGEST_LEN >> 2] = {0};
    size_t digest_len = xlnx_sha3_common_digest_size(s);

    keccak_squeeze(&s->sponge, digest_len, &digest);
    xlnx_sha3_common_write_digest(s, digest, digest_len);
}

/*
 * Handle the received data and pass it to the sponge.
 * The data is passed to the sponge in blocks of size
 * xlnx_sha3_common_block_size(s).  The data is stored in
 * s->data[] until a block is completed.
 */
static bool xlnx_sha3_common_handle_data(XlnxSha3Common *s,
    uint8_t *buf, size_t len, bool eop)
{
    size_t block_size = xlnx_sha3_common_block_size(s);
    size_t remaining = len;
    bool crossed = false;

    while (remaining) {
        if (s->data_ptr || (remaining < block_size)) {
            /*
             * Either a block has been already started, or not enough data
             * arrived to complete a block.  In any case s->data[] need to be
             * used.
             */
            if (remaining >= block_size - s->data_ptr) {
                /* Enough data to fill a block, send it to the sponge!  */
                memcpy(&s->data[s->data_ptr],
                       &buf[len - remaining],
                       block_size - s->data_ptr);

                remaining -= (block_size - s->data_ptr);
                s->data_ptr = 0;
                keccak_absorb(&s->sponge, block_size, s->data);
                crossed = true;
            } else {
                /*
                 * Not enough data remain to complete a block, store it in
                 * s->data, and wait the next DMA or complete it later with
                 * autopadding.
                 */
                memcpy(&s->data[s->data_ptr], &buf[len - remaining], remaining);
                s->data_ptr += remaining;
                remaining = 0;
            }
        } else {
            /*
             * In this case enough data remains to send a complete block to the
             * sponge.
             */
            keccak_absorb(&s->sponge, block_size, &buf[len - remaining]);
            remaining -= block_size;
            crossed = true;
        }
    }

    /*
     * Handle the automatic padding if enabled.  (See FIPS202 hexadecimal form
     * of SHA-3 padding for byte-aligned messages.)
     */
    if (eop && xlnx_sha3_common_autopadding_enabled(s)) {
        /*
         * Remaining bytes to pad until the end of the block, 2 cases might
         * occur:
         *   * Block is not terminated, then padding is added to fill the block.
         *   * Block is empty (s->data_ptr == 0) then a complete padding block
         *     is added.
         */
        remaining = block_size - s->data_ptr;
        /* Zero's part of the padding.  */
        memset(&s->data[s->data_ptr], 0, remaining);
        /* Message suffix, taken in account the current algorithm.  */
        s->data[s->data_ptr] = xlnx_sha3_common_get_padding_suffix(s);
        /* Last byte of the message.  */
        s->data[block_size - 1] |= 0x80;
        /* Give the block to the sponge and finalize.  */
        keccak_absorb(&s->sponge, block_size, s->data);
        crossed = true;
        s->data_ptr = 0;
    }
    return crossed;
}

static void xlnx_sha3_common_chain_lms_ots(XlnxSha3Common *s)
{
    int j;
    /* Get chain start and steps */
    uint8_t chain_start = xlnx_sha3_common_get_chain_start(s);
    uint8_t chain_steps = xlnx_sha3_common_get_chain_steps(s);
    size_t digest_len = xlnx_sha3_common_digest_size(s);
    uint32_t digest[XLNX_SHA3_COMMON_MAX_DIGEST_LEN >> 2] = {0};
    /*
     * Calculate digest for iteration 0.
     */
    keccak_squeeze(&s->sponge, digest_len, &digest);
    /*
     * Chaining:
     * Calculate SHAKE256 on chain_buf[0]..chain_buf[23] + digest
     * for (chain_steps - chain_start) times.
     */
    for (j = chain_start + 1; j < chain_start + chain_steps; j++) {
        /*
         * Calculate digest of previous chain iteration.
         * Note: Digest of first iteration is calculated based on stream data
         *       and not on chain_buf.
         */
        keccak_init(&s->sponge);
        s->data_ptr = 0;
        s->chain_buf[22] = j;
        xlnx_sha3_common_handle_data(s, s->chain_buf, 23, false);
        xlnx_sha3_common_handle_data(s, (uint8_t *)digest, digest_len, true);
        if (j < (chain_start + chain_steps - 1)) {
            /*
             * Digest for last iteration is not calculated here.
             * It is calculated in xlnx_sha3_common_update_digest
             * function.
             */
            keccak_squeeze(&s->sponge, digest_len, &digest);
        }
    }
    /*
     * Clear the chain_buf and cbuf_offset
     * Final digest is computed at end of xlnx_sha3_common_stream_push function
     * and not here.
     */
    memset(s->chain_buf, 0, sizeof(s->chain_buf));
    s->cbuf_offset = 0;
}

static void xlnx_sha3_common_chain_slh_dsa(XlnxSha3Common *s)
{
    uint32_t j;
    /* Get chain start and steps */
    uint8_t chain_start = xlnx_sha3_common_get_chain_start(s);
    uint8_t chain_steps = xlnx_sha3_common_get_chain_steps(s);
    size_t digest_len = xlnx_sha3_common_digest_size(s);
    uint32_t digest[XLNX_SHA3_COMMON_MAX_DIGEST_LEN >> 2] = {0};

    /*
     * Calculate digest for iteration 0.
     */
    keccak_squeeze(&s->sponge, digest_len, &digest);

    /*
     * Calculate SHAKE256 on chain_buf[0]..chain_buf[47] + digest
     */
    for (j = chain_start + 1; j < chain_start + chain_steps; j++) {
        /*
         * Calculate digest of previous chain iteration.
         * Note: Digest of first iteration is calculated based on stream data
         *       and not on chain_buf.
         */
        keccak_init(&s->sponge);
        s->data_ptr = 0;
        *(uint32_t *)&s->chain_buf[0x2c] = cpu_to_be32(j);
        xlnx_sha3_common_handle_data(s, s->chain_buf, 48, false);
        xlnx_sha3_common_handle_data(s, (uint8_t *) digest, digest_len, true);
        if (j < (chain_start + chain_steps - 1)) {
            /*
             * Digest for last iteration is not calculated here.
             * It is calculated in xlnx_sha3_common_update_digest
             * function.
             */
            keccak_squeeze(&s->sponge, digest_len, &digest);
        }
    }
    /*
     * Clear the chain_buf and cbuf_offset
     * Final digest is computed at end of xlnx_sha3_common_stream_push function
     * and not here.
     */
    memset(s->chain_buf, 0, sizeof(s->chain_buf));
    s->cbuf_offset = 0;
}

static void xlnx_sha3_common_chain_buf_copy(XlnxSha3Common *s,
                                             uint8_t *buf,
                                             size_t len)
{
    uint32_t copy_max = s->alg == SHA_MODE_SHAKE256_LMS_OTS_CHAIN ? 23 : 48;
    uint32_t copy_len;
    uint8_t chain_start = xlnx_sha3_common_get_chain_start(s);

    if (s->cbuf_offset < copy_max) {
        copy_len = s->cbuf_offset + len <= copy_max ? len :
                                    copy_max - s->cbuf_offset;
        memcpy(s->chain_buf, buf, copy_len);
        s->cbuf_offset += copy_len;
        /*
         * Insert the start chain index in backup chain_buf and
         * actual stream buf.
         */
        if (s->cbuf_offset == copy_max) {
            if (s->alg == SHA_MODE_SHAKE256_LMS_OTS_CHAIN) {
                s->chain_buf[0x16] = chain_start;
                buf[copy_len - 1] = chain_start;
            } else if (s->alg == SHA_MODE_SHAKE256_SLH_DSA_CHAIN) {
                *(uint32_t *)&s->chain_buf[0x2c] =
                    cpu_to_be32(chain_start);
                *(uint32_t *)&buf[copy_len - 4] =
                    cpu_to_be32(chain_start);
            } else {
                g_assert_not_reached();
            }
        }
    }
}

/*
 * Callback from the DMA, consume the data..  Store them in the data[] block
 * and pass them through to the sponge if a block boundary is crossed.
 */
static size_t xlnx_sha3_common_stream_push(StreamSink *obj,
                                           uint8_t *buf,
                                           size_t len,
                                           bool eop)
{
    XlnxSha3Common *s = XLNX_SHA3_COMMON(obj);

    /* Is the crypto block ready to accept data?  */
    if (s->state != XLNX_SHA3_COMMON_RUNNING) {
        hw_error("%s: crypto block in bad state %d\n",
                 object_get_canonical_path(OBJECT(s)), s->state);
        return 0;
    }

    switch (s->alg) {
    case SHA_MODE_SHAKE256_LMS_OTS_CHAIN:
        /*
         * Copy the first 23 bytes of data for chaining.
         *  0x00 - 0x10 : I
         *  0x10 - 0x14 : q
         *  0x14 - 0x16 : i
         *  0x16 - 0x17 : j (this field is updated during chaining)
         *
         *  Fallthrough
         */
    case SHA_MODE_SHAKE256_SLH_DSA_CHAIN:
        /*
         * Copy the PK.seed (16 bytes) + ADRS (32 bytes)
         *  0x00 - 0x10: PK.seed
         *  0x10 - 0x30: ADRS
         *     0x10 - 0x14: Layer addr
         *     0x14 - 0x20: Tree addr
         *     0x20 - 0x24: Type
         *     0x24 - 0x28: Key addr
         *     0x28 - 0x2c: Chain addr
         *     0x2c - 0x30: Hash addr (this field is updated during chaining)
         */
        xlnx_sha3_common_chain_buf_copy(s, buf, len);
        break;
    default:
        break;
    }

    if (xlnx_sha3_common_handle_data(s, buf, len, eop)) {
        /* Handle LMS_OTS/SLH_DSA Chaining */
        if (s->alg == SHA_MODE_SHAKE256_LMS_OTS_CHAIN) {
            xlnx_sha3_common_chain_lms_ots(s);
        } else if (s->alg == SHA_MODE_SHAKE256_SLH_DSA_CHAIN) {
            xlnx_sha3_common_chain_slh_dsa(s);
        }
        /* If we cross a block boundary, update the digest.  */
        xlnx_sha3_common_update_digest(s);
    }

    if (eop) {
        xlnx_sha3_common_handle_end_of_packet(s);
    }

    return len;
}

static void xlnx_sha3_common_class_init(ObjectClass *klass, const void *data)
{
    StreamSinkClass *ssc = STREAM_SINK_CLASS(klass);

    ssc->push = xlnx_sha3_common_stream_push;
    ssc->can_push = xlnx_sha3_common_stream_can_push;
}

static const TypeInfo xlnx_sha3_common_info = {
    .name          = TYPE_XLNX_SHA3_COMMON,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(XlnxSha3Common),
    .class_init    = xlnx_sha3_common_class_init,
    .class_size    = sizeof(XlnxSha3CommonClass),
    .interfaces = (InterfaceInfo[]) {
        { TYPE_STREAM_SINK },
        { }
    },
    .abstract = true,
};

static void xlnx_sha3_common_register_types(void)
{
    type_register_static(&xlnx_sha3_common_info);
}

type_init(xlnx_sha3_common_register_types)
