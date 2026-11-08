#pragma once

#include "Network/SharedPacket.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// LevelChunk packets shared by every player sent the same chunk revision; main thread only
class ChunkPacketCache {
public:
    struct Key {
        const void *mLevel;
        int64_t mChunk;
        const void *mContext;

        bool operator==(const Key &other) const {
            return mLevel == other.mLevel && mChunk == other.mChunk && mContext == other.mContext;
        }
    };

    struct Stamp {
        uint64_t mInstance = 0;
        uint32_t mRevision = 0;
        std::string mBlockActors;

        bool operator==(const Stamp &other) const {
            return mInstance == other.mInstance && mRevision == other.mRevision && mBlockActors == other.mBlockActors;
        }
    };

    // a join burst asks for the same chunks within a few seconds
    static const uint64_t EXPIRY_TICKS = 100;

    // encode runs only when the stamp changed since the last send of this key
    std::shared_ptr<const SharedPacket> get(const Key &key, const Stamp &stamp, uint64_t tick,
                                            const std::function<std::string()> &encode);

    void expire(uint64_t tick);

    size_t size() const { return mEntries.size(); }

private:
    struct KeyHasher {
        size_t operator()(const Key &key) const;
    };

    struct Entry {
        Stamp mStamp;
        std::shared_ptr<const SharedPacket> mPacket;
        uint64_t mLastUsed = 0;
    };

    std::unordered_map<Key, Entry, KeyHasher> mEntries;
};
