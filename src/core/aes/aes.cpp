#include "core/aes/aes.h"

namespace AES {
static int wrap_index(int i) {
    return i < 0 ? ((i % 16) + 16) % 16 : (i > 15 ? i % 16 : i);
}

static std::array<u8, 16> n128_lrot(const std::array<u8, 16>& in, u32 rot) {
    std::array<u8, 16> out;
    u32 bit_shift, byte_shift;

    rot = rot % 128;
    byte_shift = rot / 8;
    bit_shift = rot % 8;

    for (int i = 0; i < 16; i++) {
        out[i] = (in[wrap_index(i + byte_shift)] << bit_shift) |
                 (in[wrap_index(i + byte_shift + 1)] >> (8 - bit_shift));
    }
    return out;
}

static std::array<u8, 16> n128_add(const std::array<u8, 16>& a, const std::array<u8, 16>& b) {
    std::array<u8, 16> out;
    int carry = 0;
    int sum = 0;

    for (int i = 15; i >= 0; i--) {
        sum = a[i] + b[i] + carry;
        carry = sum >> 8;
        out[i] = sum & 0xff;
    }

    //?

    /*while(carry != 0){
        for(int i = 15; i >= 0; i--){
            sum = out[i] + carry;
            carry = sum >> 8;
            out[i] = sum & 0xff;
        }
    }*/
    return out;
}

static std::array<u8, 16> n128_xor(const std::array<u8, 16>& a, const std::array<u8, 16>& b) {
    std::array<u8, 16> out;
    for (int i = 0; i < 16; i++) {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

static std::array<u8, 16> MakeKey(const std::array<u8, 16>& x, const std::array<u8, 16>& y) {
    return n128_lrot(n128_add(n128_xor(n128_lrot(x, 2), y), MAKE_KEY_CONST), 87);
}

std::array<u8, 16> MakeKey(int slot, const std::array<u8, 16>& y) {
    if (slot == 0x25) {
        return MakeKey(SLOT25_KEY_X, y);
    } else if (slot == 0x2C) {
        return MakeKey(SLOT2C_KEY_X, y);
    } else {
        return std::array<u8, 16>();
    }
}

void AddCtr(std::array<u8, 16>& ctr, u32 carry) {
    u32 counter[4];
    u32 sum;

    for (int i = 0; i < 4; i++) {
        counter[i] = (ctr[i * 4 + 0] << 24) | (ctr[i * 4 + 1] << 16) | (ctr[i * 4 + 2] << 8) |
                     (ctr[i * 4 + 3] << 0);
    }

    for (int i = 3; i >= 0; i--) {
        sum = counter[i] + carry;
        if (sum < counter[i]) {
            carry = 1;
        } else {
            carry = 0;
        }
        counter[i] = sum;
    }

    for (int i = 0; i < 4; i++) {
        ctr[i * 4 + 0] = counter[i] >> 24;
        ctr[i * 4 + 1] = counter[i] >> 16;
        ctr[i * 4 + 2] = counter[i] >> 8;
        ctr[i * 4 + 3] = counter[i] >> 0;
    }
}

void AesCtrDecrypt(void* data, u64 length, const std::array<u8, 16>& key,
                   const std::array<u8, 16>& ctr) {
    u8* p = reinterpret_cast<u8*>(data);
    std::array<u8, 16> c(ctr), xorpad;
    while (length > 0) {
        xorpad = AesCipher(c, key);
        AddCtr(c, 1);
        u64 l = length > 16 ? 16 : length;
        for (u64 j = 0; j < l; ++j)
            *(p++) ^= xorpad[j];
        length -= l;
    }
}
}
