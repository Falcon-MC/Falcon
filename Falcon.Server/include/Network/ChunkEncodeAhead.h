#pragma once

#include "Network/ChunkPacketCache.h"
#include "Protocol/PacketCodecContext.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Level;
class ServerNetworkHandler;

/**
 * Encodes the LevelChunk packets the players' chunk sends will ask for this tick on worker threads and the main thread
 * together, after the world tick and before the players' tick. The workers read only copies made on the main thread.
 * The cache is filled and every packet sent on the main thread, at the same point and in the same order as without
 * it: a chunk changed since its copy no longer matches its stamp and is encoded again at its send.
 */
class ChunkEncodeAhead {
public:
    // one packet as the workers encode it: every input owned or immutable
    struct Job {
        ChunkPacketCache::Key mKey;
        ChunkPacketCache::Stamp mStamp;
        PacketCodecContext mContext;
        LevelChunkNetworkSnapshot mSnapshot;
        int32_t mChunkX = 0;
        int32_t mChunkZ = 0;
        int32_t mDimension = 0;
        std::string mEncoded;
    };

    explicit ChunkEncodeAhead(unsigned threadCount);

    ~ChunkEncodeAhead();

    ChunkEncodeAhead(const ChunkEncodeAhead &) = delete;

    ChunkEncodeAhead &operator=(const ChunkEncodeAhead &) = delete;

    unsigned getThreadCount() const {
        return (unsigned) mThreads.size();
    }

    // main thread: picks and copies the chunks, then hands them to the workers
    void start(ServerNetworkHandler &owner);

    // main thread: encodes beside the workers until every copy is done, then fills the cache
    void finish(ServerNetworkHandler &owner);

    // the bytes a send of the same chunk would make on the main thread
    static std::string encode(const Job &job);

    // block entities start() read from storage for a chunk whose block entities are not loaded; null once the
    // chunk changed or was reloaded. Main thread
    const std::string *findStoredBlockActors(const Level &level, int64_t chunk, uint64_t instance,
                                             uint32_t revision) const;

private:
    struct StoredKey {
        const Level *mLevel;
        int64_t mChunk;

        bool operator==(const StoredKey &other) const {
            return mLevel == other.mLevel && mChunk == other.mChunk;
        }
    };

    struct StoredKeyHasher {
        size_t operator()(const StoredKey &key) const;
    };

    struct StoredBlockActors {
        uint64_t mInstance = 0;
        uint32_t mRevision = 0;
        std::string mEncoded;
    };

    void _work();

    // filled and read on the main thread only, until the next start()
    std::unordered_map<StoredKey, StoredBlockActors, StoredKeyHasher> mStoredBlockActors;

    std::vector<std::thread> mThreads;
    std::mutex mMutex;
    std::condition_variable mWake;
    std::condition_variable mDone;
    std::vector<Job> mJobs;
    size_t mNext = 0;
    size_t mRemaining = 0;
    bool mStopping = false;
};
