#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

enum class RateLimitedPacket : uint8_t {
    Inbound,
    Command,
    Chat,
    FormResponse,
    Movement,
    Count
};

/**
 * Per-connection token buckets that cap how often a client may send each category of
 * packet, so a flooding client cannot monopolise the main thread.
 */
class PacketRateLimiter {
public:
    using Rates = std::array<int, (size_t) RateLimitedPacket::Count>;

    explicit PacketRateLimiter(const Rates &perSecond);

    bool tryAcquire(RateLimitedPacket category);

    bool markFlooded();

private:
    struct Bucket {
        double mRate = 0.0;
        double mTokens = 0.0;
        std::chrono::steady_clock::time_point mLastRefill;
    };

    std::array<Bucket, (size_t) RateLimitedPacket::Count> mBuckets;
    bool mFlooded = false;
};
