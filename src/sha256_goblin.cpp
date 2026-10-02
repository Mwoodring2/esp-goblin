#include "log_policy.h"
#include <cstring>

namespace {
uint32_t rotr(uint32_t value, uint32_t bits) { return (value >> bits) | (value << (32u - bits)); }
const uint32_t kSha[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};
void compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t words[64];
    for (int i = 0; i < 16; ++i) {
        words[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t small = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
        const uint32_t large = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + small + words[i - 7] + large;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t t1 = h + s1 + ch + kSha[i] + words[i];
        const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + s0 + maj;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}
}
Sha256::Sha256() { reset(); }
void Sha256::reset() {
    bits_ = 0;
    block_len_ = 0;
    state_[0] = 0x6a09e667u; state_[1] = 0xbb67ae85u; state_[2] = 0x3c6ef372u; state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu; state_[5] = 0x9b05688cu; state_[6] = 0x1f83d9abu; state_[7] = 0x5be0cd19u;
}
void Sha256::update(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    bits_ += static_cast<uint64_t>(length) * 8u;
    while (length) {
        const size_t room = 64u - block_len_;
        const size_t take = length < room ? length : room;
        std::memcpy(block_ + block_len_, bytes, take);
        block_len_ += take;
        bytes += take;
        length -= take;
        if (block_len_ == 64u) { compress(state_, block_); block_len_ = 0; }
    }
}
void Sha256::finish(uint8_t out[32]) {
    uint8_t block[64];
    std::memcpy(block, block_, block_len_);
    block[block_len_++] = 0x80;
    if (block_len_ > 56u) {
        while (block_len_ < 64u) block[block_len_++] = 0;
        compress(state_, block);
        block_len_ = 0;
    }
    while (block_len_ < 56u) block[block_len_++] = 0;
    for (int shift = 7; shift >= 0; --shift) block[block_len_++] = static_cast<uint8_t>(bits_ >> (shift * 8));
    compress(state_, block);
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>(state_[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
    }
}
void sha256Hex(const uint8_t hash[32], char out[65]) {
    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = hex[hash[i] >> 4];
        out[i * 2 + 1] = hex[hash[i] & 0x0F];
    }
    out[64] = 0;
}
