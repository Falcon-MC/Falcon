#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

struct Uuid {
    uint64_t mostSignificantBits;
    uint64_t leastSignificantBits;

    Uuid() : mostSignificantBits(0), leastSignificantBits(0) {
    }

    Uuid(uint64_t most, uint64_t least) : mostSignificantBits(most), leastSignificantBits(least) {
    }

    bool operator==(const Uuid &right) const {
        return mostSignificantBits == right.mostSignificantBits && leastSignificantBits == right.leastSignificantBits;
    }

    bool operator!=(const Uuid &right) const {
        return !(*this == right);
    }

    std::string toString() const {
        char buffer[40];
        snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
                 (unsigned int) (mostSignificantBits >> 32),
                 (unsigned int) ((mostSignificantBits >> 16) & 0xffff),
                 (unsigned int) (mostSignificantBits & 0xffff),
                 (unsigned int) ((leastSignificantBits >> 48) & 0xffff),
                 (unsigned long long) (leastSignificantBits & 0xffffffffffffULL));
        return std::string(buffer);
    }

    static Uuid fromString(const std::string &value) {
        unsigned int a = 0, b = 0, c = 0, d = 0;
        unsigned long long e = 0;
        sscanf(value.c_str(), "%08x-%04x-%04x-%04x-%012llx", &a, &b, &c, &d, &e);
        uint64_t most = ((uint64_t) a << 32) | ((uint64_t) b << 16) | (uint64_t) c;
        uint64_t least = ((uint64_t) d << 48) | (uint64_t) e;
        return Uuid(most, least);
    }
};
