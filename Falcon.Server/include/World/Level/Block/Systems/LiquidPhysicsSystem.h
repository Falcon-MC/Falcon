#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

struct LiquidInfo {
    bool water = false;
    bool lava = false;
    bool bubble = false;
    bool dragDown = false;
    bool source = false;
    bool falling = false;
    int decay = 0;
    float height = 0.0f;
};

struct LiquidChange {
    Vector3i position;
    uint32_t runtimeId = 0;
    int layer = 0;
};

class Block;
class Level;

// LiquidBlock of the game and the two block tick queues of each chunk; waterlogged water lives on block layer 1,
// every change is recorded for callers to broadcast
class LiquidPhysicsSystem {
public:
    explicit LiquidPhysicsSystem(Level &level) : mLevel(level), mRandomTicks(true) {}

    void moveStateFrom(LiquidPhysicsSystem &&other);

    // every block write lands here once the chunk holds it: place events, then neighbour updates
    void onBlockChanged(const Vector3i &position, int layer, const BlockState &previous);

    LiquidInfo getLiquidInfo(int32_t x, int32_t y, int32_t z);
    Vector3f getFlowVector(const Vector3i &position);

    // a tick for the liquid now at the position
    void schedule(const Vector3i &position, int64_t delay);

    // FillCommand writes every block with flag 2, then updates the neighbours of each in turn;
    // only the writes asked for go quiet, not what their place events write
    class QuietWrites {
    public:
        explicit QuietWrites(LiquidPhysicsSystem &system) : mSystem(system), mPrevious(system.mQuiet) {
            mSystem.mQuiet = true;
        }

        ~QuietWrites() {
            mSystem.mQuiet = mPrevious;
        }

        QuietWrites(const QuietWrites &) = delete;
        QuietWrites &operator=(const QuietWrites &) = delete;

    private:
        LiquidPhysicsSystem &mSystem;
        bool mPrevious;
    };

    // neighbour updates for the liquid side of a block written quietly
    void updateNeighbours(const Vector3i &position);

    // columns sorted by z, then x
    void tick(int64_t levelTick, const std::vector<int64_t> &columns);

    size_t getScheduledCount() const {
        return mTicks.getScheduledCount() + mRandomTicks.getScheduledCount();
    }

    size_t getLastProcessedCount() const {
        return mTicks.getLastProcessedCount() + mRandomTicks.getLastProcessedCount();
    }

    // the PendingTicks and RandomTicks records of a chunk coming in; a chunk older Falcon saved has its
    // flowing blocks rewritten first
    void loadPendingTicks(LevelChunk &chunk);

    // records refreshed from the queues; true when either changed
    bool savePendingTicks(LevelChunk &chunk) const;

    void dropPendingTicks(int32_t chunkX, int32_t chunkZ) {
        mTicks.dropColumn(chunkX, chunkZ);
        mRandomTicks.dropColumn(chunkX, chunkZ);
    }

    static const BlockState &fluidStateAt(const LevelChunk &chunk, int32_t localX, int32_t y, int32_t localZ);

    // 0: no water on layer 1; 1: placed water only; 2: flowing water enters too
    static uint8_t getWaterloggingLevel(const BlockState &state);

    void normalizeWaterlogged(const Vector3i &position);

    std::vector<LiquidChange> consumeChanges();

    // the delay of the liquid block's own ticks in this dimension
    int32_t getTickDelay(LiquidMaterial material) const;

    // what block hooks see of the world: unloaded blocks read as bedrock on layer 0; outside a tick each call
    // looks its chunk up again
    Level &getLevel() {
        return mLevel;
    }

    const BlockState &stateAt(const Vector3i &position, int layer = 0) {
        const Scope scope(*this);
        return _stateAt(position, layer);
    }

    LiquidProperties propertiesAt(const Vector3i &position, int layer = 0) {
        const Scope scope(*this);
        return _propertiesAt(position, layer);
    }

    bool isLoaded(const Vector3i &position) {
        const Scope scope(*this);
        return _isLoaded(position);
    }

    // setBlock with flag 3, or 2 without neighbour updates
    void write(const Vector3i &position, int layer, const BlockState &state, bool notify = true) {
        const Scope scope(*this);
        _write(position, layer, state, notify);
    }

    // plugins may veto; they run arbitrary code, so no chunk pointer survives the call
    bool allows(const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                const Vector3i *source = nullptr);

    void queueTick(const Vector3i &position, const Block *block, int32_t delay);
    void queueRandomTick(const Vector3i &position, const Block *block, int32_t delay);

    bool isTickPending(const Vector3i &position, const Block *block, int32_t delay) const {
        return mTicks.isPending(position, block, delay);
    }

    // any tick in either queue for the position
    bool hasTickAt(const Vector3i &position) const {
        return mTicks.hasTickAt(position) || mRandomTicks.hasTickAt(position);
    }

private:
    // clears the chunk cache on the outermost call: chunks may unload between two
    struct Scope {
        explicit Scope(LiquidPhysicsSystem &system) : mSystem(system) {
            mSystem._enter();
        }

        ~Scope() {
            --mSystem.mDepth;
        }

        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
        LiquidPhysicsSystem &mSystem;
    };

    struct ChunkSlot {
        int32_t mX = 0;
        int32_t mZ = 0;
        LevelChunk *mChunk = nullptr;
    };

    static constexpr size_t CHUNK_CACHE_SIZE = 4;

    // the slope search walks five blocks out and looks one further, at its height and one below
    static constexpr int32_t SEARCH_RADIUS = 6;
    static constexpr int32_t SEARCH_SIDE = 2 * SEARCH_RADIUS + 1;

    struct SearchSlot {
        uint32_t mStamp = 0;
        LiquidProperties mProperties;
    };

    // passes start at 1 or depth - 3 and stop at 4
    static constexpr int SLOPE_PASSES = 6;

    struct SlopeSlot {
        uint32_t mStamp = 0;
        int16_t mCost = 0;
    };

    struct SolidifyCheck {
        Vector3i position;
        Vector3i neighbor;
        const Block *self;
    };

    struct Liquid {
        const BlockState *state;
        LiquidProperties properties;
        int layer;
    };

    const BlockState &_stateAt(int32_t x, int32_t y, int32_t z, int layer = 0);
    const BlockState &_stateAt(const Vector3i &position, int layer = 0) {
        return _stateAt(position.x, position.y, position.z, layer);
    }
    LiquidProperties _propertiesAt(const Vector3i &position, int layer = 0);
    // the properties of what getLiquidBlock would answer
    LiquidProperties _liquidPropertiesAt(const Vector3i &position);
    Liquid _liquidAt(const Vector3i &position);
    int _depth(const Vector3i &position, LiquidMaterial material);
    bool _isLoaded(const Vector3i &position);
    LevelChunk *_chunkAt(int32_t chunkX, int32_t chunkZ);
    void _enter();
    // plugins and drops run arbitrary code: no chunk pointer survives them
    void _forgetChunks();

    static const Block *_blockOf(const BlockState &state);
    static const BlockState &_stillState(LiquidMaterial material, int depth);
    const Block *_stillBlock(LiquidMaterial material) const;

    void _write(const Vector3i &position, int layer, const BlockState &state, bool notify);
    void _updateNeighbors(const Vector3i &position);
    void _neighborChanged(const Vector3i &position, const Vector3i &changed);
    void _liquidNeighborChanged(const Vector3i &position, const BlockState &state);
    void _onPlace(const Vector3i &position, const LiquidProperties &placed);

    void _solidifyAround(const Vector3i &position, const Block *self);
    void _solidify(const Vector3i &position, const Vector3i &neighbor, const Block *self);
    void _fizz(const Vector3i &position);

    void _onTick(const Vector3i &position, const Block *block);
    void _tickLiquid(const Vector3i &position, LiquidMaterial self);
    bool _isLiquidBlocking(const Vector3i &target, const Vector3i &from, int face);
    void _trySpreadTo(LiquidMaterial self, const Vector3i &target, int depth, const Vector3i &from, int face);
    int _slopeDistance(LiquidMaterial self, const Vector3i &position, int pass, int from);
    int _searchSlope(LiquidMaterial self, const Vector3i &position, int pass, int from);
    void _popResources(const Vector3i &position, const BlockState &state);

    bool _isFlowable(const BlockState &state, bool lava) const;

    std::string _saveRecord(const BlockTickingQueue &queue, int32_t chunkX, int32_t chunkZ) const;
    void _loadRecord(BlockTickingQueue &queue, int32_t chunkX, int32_t chunkZ, const std::string &record);
    void _migrate(LevelChunk &chunk);

    Level &mLevel;
    std::array<ChunkSlot, CHUNK_CACHE_SIZE> mChunkCache;
    size_t mChunkCacheNext = 0;
    int mDepth = 0;
    bool mSearching = false;
    Vector3i mSearchOrigin;
    uint32_t mSearchStamp = 0;
    std::array<SearchSlot, 2 * SEARCH_SIDE * SEARCH_SIDE * 2> mSearchCache;
    std::array<SlopeSlot, SEARCH_SIDE * SEARCH_SIDE * SLOPE_PASSES * 4> mSlopeCache;
    BlockTickingQueue mTicks;
    BlockTickingQueue mRandomTicks;
    std::vector<LiquidChange> mChanges;
    bool mNotify = true;
    bool mQuiet = false;
    // the game defers solidify checks raised while one runs
    bool mSolidifying = false;
    std::deque<SolidifyCheck> mDeferredSolidify;
};
