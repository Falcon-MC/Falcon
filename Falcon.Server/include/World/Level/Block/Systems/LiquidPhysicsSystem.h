#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
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
    BlockState state;
    int layer = 0;
};

class Level;

// waterlogged water lives on block layer 1; every change is recorded for callers to broadcast
class LiquidPhysicsSystem {
public:
    explicit LiquidPhysicsSystem(Level &level) : mLevel(level) {}

    void moveStateFrom(LiquidPhysicsSystem &&other);

    void onBlockChanged(int32_t x, int32_t y, int32_t z);

    LiquidInfo getLiquidInfo(int32_t x, int32_t y, int32_t z);
    Vector3f getFlowVector(const Vector3i &position);

    void schedule(const Vector3i &position, int64_t delay = 1);

    // columns sorted by z, then x
    void tick(int64_t levelTick, const std::vector<int64_t> &columns);

    size_t getScheduledCount() const {
        return mTicks.getScheduledCount();
    }

    size_t getLastProcessedCount() const {
        return mTicks.getLastProcessedCount();
    }

    // the chunk's PendingTicks record, empty when no tick waits (sub_14381D090, sub_14381C610)
    std::string savePendingTicks(int32_t chunkX, int32_t chunkZ) const;
    void loadPendingTicks(int32_t chunkX, int32_t chunkZ, const std::string &record);

    bool hasPendingTicks(int32_t chunkX, int32_t chunkZ) const {
        return mTicks.getColumnEntries(chunkX, chunkZ) != nullptr;
    }

    void dropPendingTicks(int32_t chunkX, int32_t chunkZ) {
        mTicks.dropColumn(chunkX, chunkZ);
    }

    static const BlockState &fluidStateAt(const LevelChunk &chunk, int32_t localX, int32_t y, int32_t localZ);

    static uint8_t getWaterloggingLevel(const BlockState &state);

    void normalizeWaterlogged(const Vector3i &position);

    std::vector<LiquidChange> consumeChanges();

private:
    struct Position {
        int32_t x;
        int32_t y;
        int32_t z;

        bool operator==(const Position &other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct PositionHash {
        size_t operator()(const Position &position) const {
            size_t result = std::hash<int32_t>()(position.x);
            result ^= std::hash<int32_t>()(position.y) + (result << 6) + (result >> 2);
            result ^= std::hash<int32_t>()(position.z) + (result << 6) + (result >> 2);
            return result;
        }
    };

    bool isFluidState(const BlockState &state) const;
    bool isSameFluid(const BlockState &left, const BlockState &right) const;
    bool isFlowable(const BlockState &state, bool lava) const;
    int64_t getTickRate(const BlockState &state) const;
    const BlockState &_stateAt(int32_t x, int32_t y, int32_t z, int layer = 0);
    int _fluidLayer(int32_t x, int32_t y, int32_t z);
    const BlockState &_fluidAt(int32_t x, int32_t y, int32_t z);
    void _writeLayer(const Vector3i &position, int layer, const BlockState &state);

    bool _isLoaded(int32_t x, int32_t z) const;

    bool _canBeFlowedInto(const BlockState &state, bool lava) const;
    int _calculateFlowCost(int32_t x, int32_t y, int32_t z, int accumulatedCost, int maxCost,
                           int originOpposite, int lastOpposite, bool lava);
    void _getOptimalFlowDirections(int32_t x, int32_t y, int32_t z, int decayPerBlock, bool lava, bool out[4]);

    void scheduleNeighbors(int32_t x, int32_t y, int32_t z);
    void process(const Vector3i &position);
    void processBubbleColumn(const Vector3i &position);
    void setFluidState(const Vector3i &position, const BlockState &state);
    void harden(const Vector3i &position);
    bool resolveFluidCollision(const Vector3i &target, const BlockState &sourceState, bool downward);
    BlockState makeState(bool lava, int decay, bool falling) const;

    static constexpr int8_t FLOW_BLOCKED = -1;
    static constexpr int8_t FLOW_CAN_FLOW = 0;
    static constexpr int8_t FLOW_CAN_FLOW_DOWN = 1;

    Level &mLevel;
    BlockTickingQueue mTicks;
    std::unordered_map<Position, int8_t, PositionHash> mFlowCostVisited;
    std::vector<LiquidChange> mChanges;
};
