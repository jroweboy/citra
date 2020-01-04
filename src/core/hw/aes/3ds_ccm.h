// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mbedtls/ccm.h>

namespace HW::AES {

// Due to a mistake in Nintendo's AES implementation, the 3DS uses a nonstandard calculation for the
// hash in AES CCM, meaning we need to tweak a little bit of the algorithm.
// Since mbedtls only has provisions to let one override the full CCM module, we instead copy paste
// their en/decrypt code and tweak it to work for the 3DS

int threeds_ccm_encrypt_and_tag(mbedtls_ccm_context* ctx, size_t length, const unsigned char* iv,
                                size_t iv_len, const unsigned char* add, size_t add_len,
                                const unsigned char* input, unsigned char* output,
                                unsigned char* tag, size_t tag_len);

int threeds_ccm_auth_decrypt(mbedtls_ccm_context* ctx, size_t length, const unsigned char* iv,
                             size_t iv_len, const unsigned char* add, size_t add_len,
                             const unsigned char* input, unsigned char* output,
                             const unsigned char* tag, size_t tag_len);
} // namespace HW::AES
