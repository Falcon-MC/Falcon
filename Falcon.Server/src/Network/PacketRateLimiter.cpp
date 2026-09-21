#include "Network/PacketRateLimiter.h"

#include <algorithm>

PacketRateLimiter::PacketRateLimiter(const Rates &perSecond) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    for (size_t index = 0; index < mBuckets.size(); ++index) {
        Bucket &bucket = mBuckets[index];
        bucket.mRate = (double) perSecond[index];
        bucket.mTokens = bucket.mRate;
        bucket.mLastRefill = now;
    }
}

bool PacketRateLimiter::tryAcquire(RateLimitedPacket category) {
    Bucket &bucket = mBuckets[(size_t) category];
    if (bucket.mRate <= 0.0)
        return true;

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - bucket.mLastRefill).count();
    bucket.mLastRefill = now;
    bucket.mTokens = std::min(bucket.mRate, bucket.mTokens + elapsed * bucket.mRate);

    if (bucket.mTokens < 1.0)
        return false;

    bucket.mTokens -= 1.0;
    return true;
}

bool PacketRateLimiter::markFlooded() {
    if (mFlooded)
        return false;

    mFlooded = true;
    return true;
}
