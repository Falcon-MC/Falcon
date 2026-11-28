#include "Network/ChunkEncodeAhead.h"

#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"

#include <functional>
#include <unordered_set>
#include <utility>

namespace {
    struct KeyHasher {
        size_t operator()(const ChunkPacketCache::Key &key) const {
            size_t hash = std::hash<const void *>()(key.mLevel);
            hash = hash * 31 + std::hash<int64_t>()(key.mChunk);
            return hash * 31 + std::hash<const void *>()(key.mContext);
        }
    };
}

ChunkEncodeAhead::ChunkEncodeAhead(unsigned threadCount) {
    mThreads.reserve(threadCount);
    try {
        for (unsigned i = 0; i < threadCount; ++i)
            mThreads.emplace_back([this]() { _work(); });
    } catch (...) {
        // a joinable thread destroyed by the unwinding would end the process: the ones started stop first
        _stopWorkers();
        throw;
    }
}

ChunkEncodeAhead::~ChunkEncodeAhead() {
    _stopWorkers();
}

void ChunkEncodeAhead::_stopWorkers() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStopping = true;
    }
    mWake.notify_all();
    for (std::thread &thread: mThreads) {
        if (thread.joinable())
            thread.join();
    }
}

size_t ChunkEncodeAhead::StoredKeyHasher::operator()(const StoredKey &key) const {
    return std::hash<const void *>()(key.mLevel) * 31 + std::hash<int64_t>()(key.mChunk);
}

const std::string *ChunkEncodeAhead::findStoredBlockActors(const Level &level, int64_t chunk, uint64_t instance,
                                                           uint32_t revision) const {
    const auto found = mStoredBlockActors.find(StoredKey{&level, chunk});
    if (found == mStoredBlockActors.end() || found->second.mInstance != instance
        || found->second.mRevision != revision)
        return nullptr;
    return &found->second.mEncoded;
}

void ChunkEncodeAhead::start(ServerNetworkHandler &owner) {
    mStoredBlockActors.clear();

    struct PlayerPicks {
        Level *mLevel;
        const PacketCodecContext *mContext;
        std::vector<int64_t> mChunks;
    };
    std::vector<PlayerPicks> players;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent || !player.hasChunkPosition())
            continue;
        PlayerPicks picks{&owner.getLevelFor(player), &owner.getCodecContext(entry.first), {}};
        ChunkStreamHandler::peekNextSends(owner, player, picks.mChunks);
        if (!picks.mChunks.empty())
            players.push_back(std::move(picks));
    }

    ChunkPacketCache &cache = owner.getChunkPacketCache();
    std::vector<Job> jobs;
    size_t copiedBytes = 0;
    // players near each other ask for the same chunks
    std::unordered_set<ChunkPacketCache::Key, KeyHasher> queued;
    // one chunk per player per round: a capped batch still reaches every player's nearest chunks; whatever the cap
    // leaves is encoded at its send as before
    bool full = false;
    for (size_t round = 0; !full; ++round) {
        bool more = false;
        for (const PlayerPicks &picks: players) {
            if (round >= picks.mChunks.size())
                continue;
            more = true;
            const int64_t hash = picks.mChunks[round];
            const ChunkPacketCache::Key key{picks.mLevel, hash, picks.mContext};
            if (queued.count(key) != 0)
                continue;

            Level &level = *picks.mLevel;
            const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
            const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);
            const LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
            if (chunk == nullptr)
                continue;
            if (jobs.size() >= MAX_JOBS_PER_TICK) {
                full = true;
                break;
            }

            const bool fromStorage = !level.getBlockActors().isChunkLoaded(chunkX, chunkZ);
            ChunkPacketCache::Stamp stamp = ChunkStreamHandler::chunkStamp(owner, level, chunkX, chunkZ);
            const bool cached = cache.contains(key, stamp);
            // counted before the block actors are remembered or the chunk is copied
            const size_t retained = retainedBytes(fromStorage, cached, cached ? 0 : chunk->snapshotNetworkBytes(),
                                                  stamp.mBlockActors.size());
            if (copiedBytes + retained > MAX_COPIED_BYTES_PER_TICK) {
                full = true;
                break;
            }
            copiedBytes += retained;

            // the send of this chunk finds them here instead of reading storage a second time
            if (fromStorage)
                mStoredBlockActors[StoredKey{&level, hash}] =
                        StoredBlockActors{stamp.mInstance, stamp.mRevision, stamp.mBlockActors};
            if (cached)
                continue;
            queued.insert(key);
            jobs.push_back(Job{key, std::move(stamp), *picks.mContext, chunk->snapshotNetwork(), chunkX, chunkZ,
                               level.getDimensionId(), std::string()});
        }
        if (!more)
            break;
    }

    if (jobs.empty())
        return;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mJobs = std::move(jobs);
        mNext = 0;
        mRemaining = mJobs.size();
    }
    mWake.notify_all();
}

void ChunkEncodeAhead::finish(ServerNetworkHandler &owner) {
    std::vector<Job> jobs;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        // the main thread would only wait: it encodes as well
        while (mNext < mJobs.size()) {
            Job &job = mJobs[mNext++];
            lock.unlock();
            job.mEncoded = encode(job);
            lock.lock();
            --mRemaining;
        }
        mDone.wait(lock, [this]() { return mRemaining == 0; });
        jobs = std::move(mJobs);
        mJobs.clear();
        mNext = 0;
    }

    ChunkPacketCache &cache = owner.getChunkPacketCache();
    for (Job &job: jobs)
        cache.put(job.mKey, job.mStamp, owner.getChunkStreamTick(), std::move(job.mEncoded));
}

std::string ChunkEncodeAhead::encode(const Job &job) {
    return ChunkStreamHandler::writeChunkPacket(
            job.mChunkX, job.mChunkZ, job.mDimension, (uint32_t) job.mSnapshot.mSections.size(),
            job.mSnapshot.encode(job.mContext.getBlockNetworkIds()) + job.mStamp.mBlockActors, job.mContext);
}

void ChunkEncodeAhead::_work() {
    std::unique_lock<std::mutex> lock(mMutex);
    while (true) {
        mWake.wait(lock, [this]() { return mStopping || mNext < mJobs.size(); });
        if (mStopping)
            return;

        Job &job = mJobs[mNext++];
        lock.unlock();
        job.mEncoded = encode(job);
        lock.lock();
        if (--mRemaining == 0)
            mDone.notify_one();
    }
}
