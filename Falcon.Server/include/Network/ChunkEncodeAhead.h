#pragma once

#include "Network/ChunkPacketCache.h"
#include "Protocol/PacketCodecContext.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

    // the most a tick looks at and keeps ahead: distinct chunks stamped, cache hits included (a stamp can read the
    // block actors from storage, and a hit makes no job, so the jobs alone don't bound it), and bytes kept until
    // finish() (chunk copies and block actor payloads, counted before they are kept); the chunks past either cap are
    // stamped and encoded at their send as before
    static constexpr size_t MAX_CHUNKS_PER_TICK = 256;
    static constexpr size_t MAX_COPIED_BYTES_PER_TICK = 8 * 1024 * 1024;

    // what start() learns about a chunk from its stamp
    struct Visit {
        bool mCached = false;
        // what the chunk keeps until finish(), from retainedBytes()
        size_t mRetained = 0;
    };

    struct PlanCounts {
        size_t mChunks = 0;
        size_t mJobs = 0;
        size_t mBytes = 0;
    };

    // start()'s walk over the players' next sends, apart from the world so the caps can be tested: one chunk per
    // player per round, so a capped batch still reaches every player's nearest chunks; each key once, cache hits
    // too; it stops at the first cap reached. stamp(key) stamps a loaded chunk and returns nothing for one that
    // isn't; keep(key, visit) keeps what the stamp read, and makes a job for a chunk not in the cache
    template<typename Key, typename Hasher, typename StampFn, typename KeepFn>
    static PlanCounts plan(const std::vector<std::vector<Key>> &picks, StampFn &&stamp, KeepFn &&keep) {
        PlanCounts counts;
        std::unordered_set<Key, Hasher> seen;
        for (size_t round = 0;; ++round) {
            bool more = false;
            for (const std::vector<Key> &chunks: picks) {
                if (round >= chunks.size())
                    continue;
                more = true;
                const Key &key = chunks[round];
                if (!seen.insert(key).second)
                    continue;
                if (counts.mChunks >= MAX_CHUNKS_PER_TICK)
                    return counts;

                const std::optional<Visit> visit = stamp(key);
                if (!visit)
                    continue;
                counts.mChunks++;
                if (counts.mBytes + visit->mRetained > MAX_COPIED_BYTES_PER_TICK)
                    return counts;
                counts.mBytes += visit->mRetained;
                if (!visit->mCached)
                    counts.mJobs++;
                keep(key, *visit);
            }
            if (!more)
                return counts;
        }
    }

    // throws std::system_error when a worker can't start, after stopping the ones that did
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

    // what one chunk keeps until finish(), known before any of it is kept: the block actors read from storage are
    // remembered for its send, and a chunk not in the cache also has a job holding its copy and the block actors
    static size_t retainedBytes(bool fromStorage, bool cached, size_t snapshotBytes, size_t blockActorBytes) {
        return (fromStorage ? blockActorBytes : 0) + (cached ? 0 : snapshotBytes + blockActorBytes);
    }

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

    void _stopWorkers();

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
