#pragma once

#include <array>

#include "common/common_types.h"

namespace AES {

// secret values defined in core/aes/aes_secret.cpp
extern const std::array<u8, 16> MAKE_KEY_CONST;
extern const std::array<u8, 16> SLOT25_KEY_X;
extern const std::array<u8, 16> SLOT2C_KEY_X; // placeholder
extern const std::array<u8, 16> ZERO_KEY;

std::array<u8, 16> MakeKey(int slot, const std::array<u8, 16>& y);
void AddCtr(std::array<u8, 16>& ctr, u32 carry);
std::array<u8, 16> AesCipher(const std::array<u8, 16>& input, const std::array<u8, 16>& key);
void AesCtrDecrypt(void* data, u64 length, const std::array<u8, 16>& key,
                   const std::array<u8, 16>& ctr);

struct AesContext {
    std::array<u8, 16> key, ctr;
    bool encrypted;
    AesContext() : encrypted(false) {}
    AesContext(const std::array<u8, 16>& k, const std::array<u8, 16>& c)
        : key(k), ctr(c), encrypted(true) {}
};
}
