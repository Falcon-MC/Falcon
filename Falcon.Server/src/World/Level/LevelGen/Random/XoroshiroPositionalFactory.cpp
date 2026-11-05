#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <openssl/evp.h>

namespace {

int64_t readBigEndian(const unsigned char *bytes) {
    uint64_t value = 0;
    for (int32_t i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[i];
    }
    return static_cast<int64_t>(value);
}

}

XoroshiroPositionalFactory::XoroshiroPositionalFactory(int64_t lo, int64_t hi) : mLo(lo), mHi(hi) {}

XoroshiroRandom XoroshiroPositionalFactory::hashOf(const std::string &name) const {
    unsigned char digest[EVP_MAX_MD_SIZE] = {};
    unsigned int length = 0;
    EVP_Digest(name.data(), name.size(), digest, &length, EVP_md5(), nullptr);
    return XoroshiroRandom::fromState(readBigEndian(digest) ^ mLo, readBigEndian(digest + 8) ^ mHi);
}

XoroshiroRandom XoroshiroPositionalFactory::at(int32_t x, int32_t y, int32_t z) const {
    return XoroshiroRandom::fromState(blockSeed(x, y, z) ^ mLo, mHi);
}

int64_t XoroshiroPositionalFactory::blockSeed(int32_t x, int32_t y, int32_t z) {
    const int64_t xPart = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871U));
    uint64_t seed = static_cast<uint64_t>(xPart ^ (static_cast<int64_t>(z) * 116129781LL) ^ static_cast<int64_t>(y));
    seed = seed * seed * 42317861ULL + seed * 11ULL;
    return static_cast<int64_t>(seed) >> 16;
}
