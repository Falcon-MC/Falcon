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
    // one chunk a player's send asks for, with what start() needs to stamp and copy it; the same chunk for the same
    // codec is one pick, whoever asks
    struct PickKey {
        ChunkPacketCache::Key mKey;
        Level *mLevel;
        const PacketCodecContext *mContext;

        bool operator==(const PickKey &other) const {
            return mKey == other.mKey;
        }
    };

    struct PickKeyHasher {
        size_t operator()(const PickKey &pick) const {
            size_t hash = std::hash<const void *>()(pick.mKey.mLevel);
            hash = hash * 31 + std::hash<int64_t>()(pick.mKey.mChunk);
            return hash * 31 + std::hash<const void *>()(pick.mKey.mContext);
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

    std::vector<std::vector<PickKey>> keys;
    keys.reserve(players.size());
    for (const PlayerPicks &picks: players) {
        std::vector<PickKey> &mine = keys.emplace_back();
        mine.reserve(picks.mChunks.size());
        for (int64_t hash: picks.mChunks)
            mine.push_back(
                    PickKey{ChunkPacketCache::Key{picks.mLevel, hash, picks.mContext}, picks.mLevel, picks.mContext});
    }

    ChunkPacketCache &cache = owner.getChunkPacketCache();
    std::vector<Job> jobs;
    // what the last stamp read, kept by keep() right after it
    const LevelChunk *chunk = nullptr;
    bool fromStorage = false;
    ChunkPacketCache::Stamp stamp;
    plan<PickKey, PickKeyHasher>(
            keys,
            [&](const PickKey &pick) -> std::optional<Visit> {
                Level &level = *pick.mLevel;
                const int32_t chunkX = ChunkStreamHandler::unpackChunkX(pick.mKey.mChunk);
                const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(pick.mKey.mChunk);
                chunk = level.peekChunkPtr(chunkX, chunkZ);
                if (chunk == nullptr)
                    return std::nullopt;
                fromStorage = !level.getBlockActors().isChunkLoaded(chunkX, chunkZ);
                stamp = ChunkStreamHandler::chunkStamp(owner, level, chunkX, chunkZ);
                Visit visit;
                visit.mCached = cache.contains(pick.mKey, stamp);
                // counted before the block actors are remembered or the chunk is copied
                visit.mRetained = retainedBytes(fromStorage, visit.mCached,
                                                visit.mCached ? 0 : chunk->snapshotNetworkBytes(),
                                                stamp.mBlockActors.size());
                return visit;
            },
            [&](const PickKey &pick, const Visit &visit) {
                Level &level = *pick.mLevel;
                const ChunkPacketCache::Key &key = pick.mKey;
                // the send of this chunk finds them here instead of reading storage a second time
                if (fromStorage)
                    mStoredBlockActors[StoredKey{&level, key.mChunk}] =
                            StoredBlockActors{stamp.mInstance, stamp.mRevision, stamp.mBlockActors};
                if (visit.mCached)
                    return;
                jobs.push_back(Job{key, std::move(stamp), *pick.mContext, chunk->snapshotNetwork(),
                                   ChunkStreamHandler::unpackChunkX(key.mChunk),
                                   ChunkStreamHandler::unpackChunkZ(key.mChunk), level.getDimensionId(),
                                   std::string()});
            });

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
