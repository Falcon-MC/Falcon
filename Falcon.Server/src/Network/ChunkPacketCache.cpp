#include "Network/ChunkPacketCache.h"

size_t ChunkPacketCache::KeyHasher::operator()(const Key &key) const {
    size_t hash = std::hash<const void *>()(key.mLevel);
    hash = hash * 31 + std::hash<int64_t>()(key.mChunk);
    return hash * 31 + std::hash<const void *>()(key.mContext);
}

std::shared_ptr<const SharedPacket> ChunkPacketCache::get(const Key &key, const Stamp &stamp, uint64_t tick,
                                                          const std::function<std::string()> &encode) {
    Entry &entry = mEntries[key];
    entry.mLastUsed = tick;
    if (entry.mPacket == nullptr || !(entry.mStamp == stamp)) {
        entry.mStamp = stamp;
        entry.mPacket = std::make_shared<const SharedPacket>(encode());
    }
    return entry.mPacket;
}

void ChunkPacketCache::expire(uint64_t tick) {
    for (auto it = mEntries.begin(); it != mEntries.end();) {
        if (tick - it->second.mLastUsed > EXPIRY_TICKS)
            it = mEntries.erase(it);
        else
            ++it;
    }
}
