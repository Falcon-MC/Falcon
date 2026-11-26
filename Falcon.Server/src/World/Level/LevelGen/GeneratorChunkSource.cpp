#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LightSystem.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <tuple>
#include <utility>

namespace {
    int32_t slotOf(int32_t offsetX, int32_t offsetZ) {
        return (offsetX + 1) * 3 + (offsetZ + 1);
    }

    // decorationClass first; one class never overlaps itself but through a chunk two apart: x, then z
    bool appliesBefore(int32_t ax, int32_t az, int32_t bx, int32_t bz) {
        return std::make_tuple(TerrainCache::decorationClass(ax, az), ax, az)
               < std::make_tuple(TerrainCache::decorationClass(bx, bz), bx, bz);
    }

    // first before and last after of each block, in index order; a block put back as it was is no change
    void addNetWrites(ChunkDecoration &decoration, size_t slot, std::vector<ChunkWrite> &log) {
        std::stable_sort(log.begin(), log.end(),
                         [](const ChunkWrite &a, const ChunkWrite &b) { return a.mIndex < b.mIndex; });
        for (size_t first = 0; first < log.size();) {
            size_t last = first;
            while (last + 1 < log.size() && log[last + 1].mIndex == log[first].mIndex)
                last++;
            if (log[first].mBefore != log[last].mAfter)
                decoration.add(slot, {log[first].mIndex, log[first].mBefore, log[last].mAfter});
            first = last + 1;
        }
    }

    void unpack(uint32_t index, int &x, int32_t &y, int &z, int &layer) {
        x = (int) (index & 15);
        z = (int) ((index >> 4) & 15);
        y = (int32_t) ((index >> 8) & 511) + LevelChunk::MIN_Y;
        layer = (int) (index >> 17);
    }

    std::tuple<int32_t, int32_t, int32_t> positionOf(const Tag &blockActor) {
        return std::make_tuple(blockActor.getInt("x"), blockActor.getInt("y"), blockActor.getInt("z"));
    }
}

void ChunkDecoration::add(size_t slot, const ChunkWrite &write) {
    const auto number = [this](const BlockState *state) {
        const auto found = std::find(mStates.begin(), mStates.end(), state);
        if (found != mStates.end())
            return (uint16_t) (found - mStates.begin());
        mStates.push_back(state);
        return (uint16_t) (mStates.size() - 1);
    };
    mWrites[slot].push_back({write.mIndex, number(write.mBefore), number(write.mAfter)});
}

size_t ChunkDecoration::bytes() const {
    size_t total = sizeof(ChunkDecoration) + mStates.size() * sizeof(const BlockState *) + mPendingTicks.size();
    for (const std::vector<Write> &writes: mWrites)
        total += writes.size() * sizeof(Write);
    // tags are small trees: a rough share each
    return total + (mBlockActors.size() + mEntities.size()) * 512;
}

TerrainCache::TerrainCache(const ChunkGenerator &generator, Decoration decoration)
        : mGenerator(generator), mDecoration(std::move(decoration)) {}

int32_t TerrainCache::decorationClass(int32_t chunkX, int32_t chunkZ) {
    static const int32_t CLASSES[2][2] = {{0, 1}, {3, 2}};
    return CLASSES[chunkZ & 1][chunkX & 1];
}

TerrainCache::GenerationClaim::~GenerationClaim() {
    {
        std::lock_guard<std::mutex> lock(mCache.mMutex);
        mCache.mGenerating.erase(mKey);
    }
    mCache.mPublished.notify_all();
}

int64_t TerrainCache::_packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

LevelChunk TerrainCache::terrain(int32_t chunkX, int32_t chunkZ) {
    return *_get(chunkX, chunkZ);
}

std::shared_ptr<const LevelChunk> TerrainCache::sharedTerrain(int32_t chunkX, int32_t chunkZ) {
    return _get(chunkX, chunkZ);
}

std::shared_ptr<const LevelChunk> TerrainCache::_get(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    {
        std::unique_lock<std::mutex> lock(mMutex);
        while (true) {
            const auto found = mChunks.find(key);
            if (found != mChunks.end()) {
                mOrder.splice(mOrder.end(), mOrder, found->second.mOrder);
                return found->second.mTerrain;
            }
            if (mGenerating.count(key) == 0)
                break;

            mPublished.wait(lock);
        }
        mGenerating.insert(key);
    }

    const GenerationClaim claim{*this, key};
    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mGenerator.getDimensionType());
    mGenerator.generate(chunk);
    mTerrainGenerations++;
    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(std::move(chunk));

    std::lock_guard<std::mutex> lock(mMutex);
    mOrder.push_back(key);
    Entry &entry = mChunks[key];
    entry.mTerrain = shared;
    entry.mOrder = std::prev(mOrder.end());
    while (mOrder.size() > MAX_CACHED_CHUNKS) {
        mChunks.erase(mOrder.front());
        mOrder.pop_front();
    }
    return shared;
}

void TerrainCache::prepareTerrain(int32_t chunkX, int32_t chunkZ) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const int64_t key = _packChunk(chunkX, chunkZ);
        if (mChunks.count(key) != 0 || mGenerating.count(key) != 0)
            return;
    }
    _get(chunkX, chunkZ);
}

bool TerrainCache::hasTerrain(int32_t chunkX, int32_t chunkZ) {
    std::lock_guard<std::mutex> lock(mMutex);
    const int64_t key = _packChunk(chunkX, chunkZ);
    return mChunks.count(key) != 0 || mGenerating.count(key) != 0;
}

void TerrainCache::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mChunks.clear();
    mOrder.clear();
    mDecorations.clear();
    mDecorationOrder.clear();
    mDecorationBytes = 0;
}

std::shared_ptr<const ChunkDecoration> TerrainCache::findDecoration(int32_t chunkX, int32_t chunkZ) {
    std::lock_guard<std::mutex> lock(mMutex);
    const auto found = mDecorations.find(_packChunk(chunkX, chunkZ));
    if (found == mDecorations.end())
        return nullptr;

    mDecorationOrder.splice(mDecorationOrder.end(), mDecorationOrder, found->second.mOrder);
    return found->second.mDecoration;
}

bool TerrainCache::claimDecoration(int32_t chunkX, int32_t chunkZ, std::shared_ptr<const ChunkDecoration> &found,
                                   bool wait) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    std::unique_lock<std::mutex> lock(mMutex);
    while (true) {
        const auto cached = mDecorations.find(key);
        if (cached != mDecorations.end()) {
            found = cached->second.mDecoration;
            return false;
        }
        if (mDecorating.insert(key).second)
            return true;
        if (!wait) {
            found = nullptr;
            return false;
        }

        mDecorated.wait(lock);
    }
}

void TerrainCache::storeDecoration(int32_t chunkX, int32_t chunkZ, std::shared_ptr<const ChunkDecoration> decoration) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDecorating.erase(key);
        if (decoration != nullptr && mDecorations.find(key) == mDecorations.end()) {
            mDecorationCount++;
            mDecorationBytes += decoration->bytes();
            mDecorationOrder.push_back(key);
            DecorationEntry &entry = mDecorations[key];
            entry.mDecoration = std::move(decoration);
            entry.mOrder = std::prev(mDecorationOrder.end());
            // evicted ones are decorated again when asked for: same result, more time
            while (mDecorationBytes > MAX_DECORATION_BYTES && mDecorationOrder.size() > 1) {
                const auto oldest = mDecorations.find(mDecorationOrder.front());
                mDecorationBytes -= oldest->second.mDecoration->bytes();
                mDecorations.erase(oldest);
                mDecorationOrder.pop_front();
            }
        }
    }
    mDecorated.notify_all();
}

void TerrainCache::awaitDecoration(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    mDecorated.wait_for(lock, timeout);
}

bool TerrainCache::findStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t &value) {
    std::lock_guard<std::mutex> lock(mStartMutex);
    const auto found = mStartValues.find(std::make_tuple(structure, startX, startZ));
    if (found == mStartValues.end())
        return false;

    value = found->second;
    return true;
}

int32_t TerrainCache::storeStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t value) {
    std::lock_guard<std::mutex> lock(mStartMutex);
    return mStartValues.emplace(std::make_tuple(structure, startX, startZ), value).first->second;
}

GeneratorChunkSource::GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache)
        : mScratch("generator", 1, seed, dimension), mCache(cache) {
    mScratch.setTerrainCache(&cache);
}

void GeneratorChunkSource::apply(LevelChunk &chunk, const ChunkDecoration &decoration, size_t slot) {
    for (const ChunkDecoration::Write &write: decoration.mWrites[slot]) {
        int x = 0, z = 0, layer = 0;
        int32_t y = 0;
        unpack(write.mIndex, x, y, z, layer);
        if (&chunk.getBlock(x, y, z, layer) == decoration.mStates[write.mBefore])
            chunk.setBlock(x, y, z, layer, *decoration.mStates[write.mAfter]);
    }
}

std::vector<GeneratorChunkSource::Earlier> GeneratorChunkSource::_earlier(int32_t chunkX, int32_t chunkZ) {
    // the earlier classes around it; one two chunks away also writes into the chunk between, but only a feature
    // reaching 16 blocks out of each would meet it there, and reading it would cost a fresh chunk 3 times the work
    const int32_t own = TerrainCache::decorationClass(chunkX, chunkZ);
    std::vector<Earlier> earlier;
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            if (TerrainCache::decorationClass(chunkX + offsetX, chunkZ + offsetZ) < own)
                earlier.push_back({chunkX + offsetX, chunkZ + offsetZ, nullptr});
        }
    }
    return earlier;
}

void GeneratorChunkSource::_resolve(std::vector<Earlier> &chunks) {
    for (Earlier &chunk: chunks)
        chunk.mDecoration = _decoration(chunk.mX, chunk.mZ, false);
    for (Earlier &chunk: chunks) {
        if (chunk.mDecoration == nullptr)
            chunk.mDecoration = _decoration(chunk.mX, chunk.mZ, true);
    }
    std::sort(chunks.begin(), chunks.end(),
              [](const Earlier &a, const Earlier &b) { return appliesBefore(a.mX, a.mZ, b.mX, b.mZ); });
}

std::vector<GeneratorChunkSource::Earlier> GeneratorChunkSource::_around(int32_t chunkX, int32_t chunkZ) {
    std::vector<Earlier> around;
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++)
            around.push_back({chunkX + offsetX, chunkZ + offsetZ, nullptr});
    }
    _resolve(around);
    return around;
}

// a claim is held while the earlier classes decorate: waits only ever go to an earlier class, never in a circle
std::shared_ptr<const ChunkDecoration> GeneratorChunkSource::_decoration(int32_t chunkX, int32_t chunkZ, bool wait) {
    std::shared_ptr<const ChunkDecoration> found = mCache.findDecoration(chunkX, chunkZ);
    if (found != nullptr || !mCache.claimDecoration(chunkX, chunkZ, found, wait))
        return found;

    std::shared_ptr<const ChunkDecoration> decorated;
    try {
        // decorated first: the scratch level holds one at a time
        std::vector<Earlier> earlier = _earlier(chunkX, chunkZ);
        _resolve(earlier);
        decorated = _decorate(chunkX, chunkZ, earlier);
    } catch (...) {
        mCache.storeDecoration(chunkX, chunkZ, nullptr);
        throw;
    }
    mCache.storeDecoration(chunkX, chunkZ, decorated);
    return decorated;
}

std::shared_ptr<const ChunkDecoration> GeneratorChunkSource::_decorate(int32_t chunkX, int32_t chunkZ,
                                                                       const std::vector<Earlier> &earlier) {
    const int32_t own = TerrainCache::decorationClass(chunkX, chunkZ);
    // neighbours first, the centre last
    std::array<std::pair<int32_t, int32_t>, 9> slots;
    size_t count = 0;
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            if (offsetX != 0 || offsetZ != 0)
                slots[count++] = {offsetX, offsetZ};
        }
    }
    slots[count] = {0, 0};

    // the scratch level empty again, even when a feature throws
    struct Unstage {
        Level &mScratch;
        int32_t mChunkX;
        int32_t mChunkZ;

        ~Unstage() {
            for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
                for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                    mScratch.getBlockActors().unloadChunk(mChunkX + offsetX, mChunkZ + offsetZ);
                    mScratch.dropChunk(mChunkX + offsetX, mChunkZ + offsetZ);
                }
            }
            mScratch.takeGeneratedEntities();
        }
    } unstage{mScratch, chunkX, chunkZ};
    std::array<std::vector<ChunkWrite>, 9> logs;
    std::array<LevelChunk *, 9> staged{};
    for (const auto &slot: slots) {
        const int32_t x = chunkX + slot.first;
        const int32_t z = chunkZ + slot.second;
        LevelChunk chunk = mCache.terrain(x, z);
        for (const Earlier &other: earlier) {
            if (std::abs(other.mX - x) <= 1 && std::abs(other.mZ - z) <= 1)
                apply(chunk, *other.mDecoration, (size_t) slotOf(x - other.mX, z - other.mZ));
        }
        // the decorated ones: structure pieces read their blocks
        chunk.setPopulated(TerrainCache::decorationClass(x, z) < own);
        staged[(size_t) slotOf(slot.first, slot.second)] = &mScratch.insertChunk(std::move(chunk));
    }
    for (size_t slot = 0; slot < 9; slot++)
        staged[slot]->setWriteLog(&logs[slot]);

    LevelChunk &center = *staged[(size_t) slotOf(0, 0)];
    if (mCache.getDecoration())
        mCache.getDecoration()(mScratch, center);
    else
        mScratch.decorate(center, nullptr);

    std::shared_ptr<ChunkDecoration> decorated = std::make_shared<ChunkDecoration>();
    BlockActorStore &generatedActors = mScratch.getBlockActors();
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            const size_t slot = (size_t) slotOf(offsetX, offsetZ);
            staged[slot]->setWriteLog(nullptr);
            addNetWrites(*decorated, slot, logs[slot]);
            std::vector<Tag> actors = generatedActors.saveChunk(chunkX + offsetX, chunkZ + offsetZ);
            decorated->mBlockActors.insert(decorated->mBlockActors.end(), std::make_move_iterator(actors.begin()),
                                           std::make_move_iterator(actors.end()));
        }
    }
    std::sort(decorated->mBlockActors.begin(), decorated->mBlockActors.end(),
              [](const Tag &a, const Tag &b) { return positionOf(a) < positionOf(b); });
    decorated->mEntities = mScratch.takeGeneratedEntities();
    decorated->mStructures = center.getStructureBounds();
    decorated->mPendingTicks = center.getPendingTicks();
    return decorated;
}

LevelChunk GeneratorChunkSource::generate(int32_t chunkX, int32_t chunkZ, std::vector<Tag> &blockActors,
                                          std::vector<Tag> &entities, uint16_t skipped) {
    // held until the chunk is finished: its decorations read many other chunks' terrain meanwhile
    const std::shared_ptr<const LevelChunk> terrain = mCache.sharedTerrain(chunkX, chunkZ);
    LevelChunk chunk = *terrain;
    _finish(chunk, blockActors, entities, skipped);
    return chunk;
}

void GeneratorChunkSource::populate(LevelChunk &chunk, std::vector<Tag> &blockActors, std::vector<Tag> &entities,
                                    uint16_t skipped) {
    _finish(chunk, blockActors, entities, skipped);
}

std::vector<GeneratorChunkSource::Need> GeneratorChunkSource::missing(int32_t chunkX, int32_t chunkZ) {
    std::vector<Need> decorations;
    std::unordered_set<int64_t> seen;
    const auto pack = [](int32_t x, int32_t z) { return ((int64_t) x << 32) | (uint32_t) z; };
    std::vector<std::pair<int32_t, int32_t>> pending;
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++)
            pending.emplace_back(chunkX + offsetX, chunkZ + offsetZ);
    }
    // a cached decoration needs nothing more
    while (!pending.empty()) {
        const std::pair<int32_t, int32_t> at = pending.back();
        pending.pop_back();
        if (!seen.insert(pack(at.first, at.second)).second || mCache.findDecoration(at.first, at.second) != nullptr)
            continue;

        decorations.push_back({at.first, at.second, true});
        const int32_t own = TerrainCache::decorationClass(at.first, at.second);
        for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
            for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                if (TerrainCache::decorationClass(at.first + offsetX, at.second + offsetZ) < own)
                    pending.emplace_back(at.first + offsetX, at.second + offsetZ);
            }
        }
    }

    std::vector<Need> needs;
    std::unordered_set<int64_t> terrains;
    for (const Need &decoration: decorations) {
        for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
            for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                const int32_t x = decoration.mX + offsetX;
                const int32_t z = decoration.mZ + offsetZ;
                if (terrains.insert(pack(x, z)).second && !mCache.hasTerrain(x, z))
                    needs.push_back({x, z, false});
            }
        }
    }
    std::stable_sort(decorations.begin(), decorations.end(), [](const Need &a, const Need &b) {
        return appliesBefore(a.mX, a.mZ, b.mX, b.mZ);
    });
    needs.insert(needs.end(), decorations.begin(), decorations.end());
    return needs;
}

bool GeneratorChunkSource::prepare(const Need &need) {
    if (!need.mDecoration) {
        mCache.prepareTerrain(need.mX, need.mZ);
        return true;
    }

    std::shared_ptr<const ChunkDecoration> found = mCache.findDecoration(need.mX, need.mZ);
    if (found != nullptr || !mCache.claimDecoration(need.mX, need.mZ, found, false))
        return true;

    // no wait on another worker's decoration: this one is released and taken again once they are done
    std::shared_ptr<const ChunkDecoration> decorated;
    try {
        std::vector<Earlier> earlier = _earlier(need.mX, need.mZ);
        bool ready = true;
        for (Earlier &chunk: earlier) {
            chunk.mDecoration = _decoration(chunk.mX, chunk.mZ, false);
            ready = ready && chunk.mDecoration != nullptr;
        }
        if (ready) {
            std::sort(earlier.begin(), earlier.end(),
                      [](const Earlier &a, const Earlier &b) { return appliesBefore(a.mX, a.mZ, b.mX, b.mZ); });
            decorated = _decorate(need.mX, need.mZ, earlier);
        }
    } catch (...) {
        mCache.storeDecoration(need.mX, need.mZ, nullptr);
        throw;
    }
    mCache.storeDecoration(need.mX, need.mZ, decorated);
    return decorated != nullptr;
}

void GeneratorChunkSource::_finish(LevelChunk &chunk, std::vector<Tag> &blockActors, std::vector<Tag> &entities,
                                   uint16_t skipped) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    std::vector<Earlier> around = _around(chunkX, chunkZ);
    around.erase(std::remove_if(around.begin(), around.end(),
                                [&](const Earlier &other) {
                                    return (skipped >> slotOf(other.mX - chunkX, other.mZ - chunkZ) & 1) != 0;
                                }),
                 around.end());

    // a block actor stays while its block is its decoration's: its own write placed, no later one over it; one
    // per decoration, two may place one at a position
    struct Actor {
        size_t mIndex;
        const Tag *mTag;
        bool mKept;
    };
    std::map<std::tuple<int32_t, int32_t, int32_t>, std::vector<Actor>> actors;
    for (size_t index = 0; index < around.size(); index++) {
        for (const Tag &actor: around[index].mDecoration->mBlockActors) {
            const auto position = positionOf(actor);
            if ((std::get<0>(position) >> 4) == chunkX && (std::get<2>(position) >> 4) == chunkZ)
                actors[position].push_back({index, &actor, true});
        }
    }

    for (size_t index = 0; index < around.size(); index++) {
        const Earlier &other = around[index];
        const size_t slot = (size_t) slotOf(chunkX - other.mX, chunkZ - other.mZ);
        if (actors.empty()) {
            apply(chunk, *other.mDecoration, slot);
            continue;
        }
        for (size_t write = 0; write < other.mDecoration->mWrites[slot].size(); write++) {
            const ChunkWrite placing = other.mDecoration->get(slot, write);
            int x = 0, z = 0, layer = 0;
            int32_t y = 0;
            unpack(placing.mIndex, x, y, z, layer);
            const bool placed = &chunk.getBlock(x, y, z, layer) == placing.mBefore;
            if (placed)
                chunk.setBlock(x, y, z, layer, *placing.mAfter);
            if (layer != 0)
                continue;
            const auto found = actors.find(std::make_tuple(chunkX * 16 + x, y, chunkZ * 16 + z));
            if (found == actors.end())
                continue;
            for (Actor &actor: found->second) {
                if (placed ? index > actor.mIndex : index == actor.mIndex)
                    actor.mKept = false;
            }
        }
    }

    // the latest kept one: two kept at one position both placed nothing there, the later decoration ran last
    for (const auto &position: actors) {
        const Actor *kept = nullptr;
        for (const Actor &actor: position.second) {
            if (actor.mKept && (kept == nullptr || actor.mIndex > kept->mIndex))
                kept = &actor;
        }
        if (kept != nullptr)
            blockActors.push_back(*kept->mTag);
    }

    for (const Earlier &other: around) {
        if (other.mX != chunkX || other.mZ != chunkZ)
            continue;
        entities = other.mDecoration->mEntities;
        chunk.getStructureBounds() = other.mDecoration->mStructures;
        if (!other.mDecoration->mPendingTicks.empty())
            chunk.setPendingTicks(other.mDecoration->mPendingTicks);
    }
    if (!chunk.hasHeightmap())
        LightSystem::computeHeightmap(chunk);
    chunk.setPopulated(true);
}
