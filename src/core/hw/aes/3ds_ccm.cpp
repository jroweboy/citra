// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

/*
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: GPL-2.0
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#include <cstring>
#include <mbedtls/ccm.h>
#include <mbedtls/cipher.h>
#include <mbedtls/error.h>
#include <mbedtls/platform_util.h>
#include "common/alignment.h"
#include "core/hw/aes/3ds_ccm.h"
#include "core/hw/aes/key.h"

/*
 * Macros for common operations.
 * Results in smaller compiled code than static inline functions.
 */

#define CCM_VALIDATE_RET(cond) MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_CCM_BAD_INPUT)
#define CCM_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#define CCM_ENCRYPT 0
#define CCM_DECRYPT 1

/*
 * Update the CBC-MAC state in y using a block in b
 * (Always using b as the source helps the compiler optimise a bit better.)
 */
#define UPDATE_CBC_MAC                                                                             \
    for (i = 0; i < 16; i++)                                                                       \
        y[i] ^= b[i];                                                                              \
                                                                                                   \
    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, y, 16, y, &olen)) != 0)                     \
        return (ret);

/*
 * Encrypt or decrypt a partial block with CTR
 * Warning: using b for temporary storage! src and dst must not be b!
 * This avoids allocating one more 16 bytes buffer while allowing src == dst.
 */
#define CTR_CRYPT(dst, src, len)                                                                   \
    do {                                                                                           \
        if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctr, 16, b, &olen)) != 0) {             \
            return (ret);                                                                          \
        }                                                                                          \
                                                                                                   \
        for (i = 0; i < (len); i++)                                                                \
            (dst)[i] = (src)[i] ^ b[i];                                                            \
    } while (0)

namespace HW::AES {

/*
 * Authenticated encryption or decryption
 */
static int threeds_auth_crypt(mbedtls_ccm_context* ctx, int mode, size_t length,
                              const unsigned char* iv, size_t iv_len, const unsigned char* add,
                              size_t add_len, const unsigned char* input, unsigned char* output,
                              unsigned char* tag, size_t tag_len) {
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    unsigned char q;
    size_t len_left, olen;
    unsigned char b[16];
    unsigned char y[16];
    unsigned char ctr[16];
    const unsigned char* src;
    unsigned char* dst;

    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
    if (tag_len == 2 || tag_len > 16 || tag_len % 2 != 0)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    /* Also implies q is within bounds */
    if (iv_len < 7 || iv_len > 13)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    if (add_len > 0xFF00)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    q = 16 - 1 - (unsigned char)iv_len;

    /*
     * First block B_0:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
    b[0] = 0;
    b[0] |= (add_len > 0) << 6;
    b[0] |= ((tag_len - 2) / 2) << 3;
    b[0] |= q - 1;

    std::memcpy(b + 1, iv, iv_len);

    /// CHANGED: AlignUp to AES block size just for writing the length in the B_0
    for (i = 0, len_left = Common::AlignUp(length, AES_BLOCK_SIZE); i < q; i++, len_left >>= 8)
        b[15 - i] = (unsigned char)(len_left & 0xFF);

    if (len_left > 0)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    /* Start CBC-MAC with first block */
    std::memset(y, 0, 16);
    UPDATE_CBC_MAC;

    /*
     * If there is additional data, update CBC-MAC with
     * add_len, add, 0 (padding to a block boundary)
     */
    if (add_len > 0) {
        size_t use_len;
        len_left = add_len;
        src = add;

        std::memset(b, 0, 16);
        b[0] = (unsigned char)((add_len >> 8) & 0xFF);
        b[1] = (unsigned char)((add_len)&0xFF);

        use_len = len_left < 16 - 2 ? len_left : 16 - 2;
        std::memcpy(b + 2, src, use_len);
        len_left -= use_len;
        src += use_len;

        UPDATE_CBC_MAC;

        while (len_left > 0) {
            use_len = len_left > 16 ? 16 : len_left;

            std::memset(b, 0, 16);
            std::memcpy(b, src, use_len);
            UPDATE_CBC_MAC;

            len_left -= use_len;
            src += use_len;
        }
    }

    /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
    ctr[0] = q - 1;
    std::memcpy(ctr + 1, iv, iv_len);
    std::memset(ctr + 1 + iv_len, 0, q);
    ctr[15] = 1;

    /*
     * Authenticate and {en,de}crypt the message.
     *
     * The only difference between encryption and decryption is
     * the respective order of authentication and {en,de}cryption.
     */
    len_left = length;
    src = input;
    dst = output;

    while (len_left > 0) {
        size_t use_len = len_left > 16 ? 16 : len_left;

        if (mode == CCM_ENCRYPT) {
            std::memset(b, 0, 16);
            std::memcpy(b, src, use_len);
            UPDATE_CBC_MAC;
        }

        CTR_CRYPT(dst, src, use_len);

        if (mode == CCM_DECRYPT) {
            std::memset(b, 0, 16);
            std::memcpy(b, dst, use_len);
            UPDATE_CBC_MAC;
        }

        dst += use_len;
        src += use_len;
        len_left -= use_len;

        /*
         * Increment counter.
         * No need to check for overflow thanks to the length check above.
         */
        for (i = 0; i < q; i++)
            if (++ctr[15 - i] != 0)
                break;
    }

    /*
     * Authentication: reset counter and crypt/mask internal tag
     */
    for (i = 0; i < q; i++)
        ctr[15 - i] = 0;

    CTR_CRYPT(y, y, 16);
    std::memcpy(tag, y, tag_len);

    return (0);
}

int threeds_ccm_encrypt_and_tag(mbedtls_ccm_context* ctx, size_t length, const unsigned char* iv,
                                size_t iv_len, const unsigned char* add, size_t add_len,
                                const unsigned char* input, unsigned char* output,
                                unsigned char* tag, size_t tag_len) {
    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);
    if (tag_len == 0)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    return (threeds_auth_crypt(ctx, CCM_ENCRYPT, length, iv, iv_len, add, add_len, input, output,
                               tag, tag_len));
}

int threeds_ccm_auth_decrypt(mbedtls_ccm_context* ctx, size_t length, const unsigned char* iv,
                             size_t iv_len, const unsigned char* add, size_t add_len,
                             const unsigned char* input, unsigned char* output,
                             const unsigned char* tag, size_t tag_len) {

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];
    unsigned char i;
    int diff;

    CCM_VALIDATE_RET(ctx != NULL);
    CCM_VALIDATE_RET(iv != NULL);
    CCM_VALIDATE_RET(add_len == 0 || add != NULL);
    CCM_VALIDATE_RET(length == 0 || input != NULL);
    CCM_VALIDATE_RET(length == 0 || output != NULL);
    CCM_VALIDATE_RET(tag_len == 0 || tag != NULL);

    if ((ret = threeds_auth_crypt(ctx, CCM_DECRYPT, length, iv, iv_len, add, add_len, input, output,
                                  check_tag, tag_len)) != 0) {
        return (ret);
    }

    /* Check tag in "constant-time" */
    for (diff = 0, i = 0; i < tag_len; i++)
        diff |= tag[i] ^ check_tag[i];

    if (diff != 0) {
        mbedtls_platform_zeroize(output, length);
        return (MBEDTLS_ERR_CCM_AUTH_FAILED);
    }

    return (0);
}
} // namespace HW::AES
