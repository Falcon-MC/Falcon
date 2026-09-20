#pragma once

#include "Core/Math/Vector3i.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

class BlockUpdateScheduler {
public:
    using UpdateHandler = std::function<void(const Vector3i &)>;

    void moveStateFrom(BlockUpdateScheduler &&other);

    void schedule(const Vector3i &position, int64_t delay);

    void cancel(const Vector3i &position);

    bool isScheduled(const Vector3i &position) const;

    void park(int32_t chunkX, int32_t chunkZ, const Vector3i &position);

    void activateColumn(int32_t chunkX, int32_t chunkZ);

    void tick(const UpdateHandler &handler, const std::function<bool(int32_t, int32_t)> &isActive);

    int64_t getCurrentTick() const { return mTick; }

    size_t getScheduledCount() const { return mSchedule.size(); }

    size_t getLastProcessedCount() const { return mLastProcessed; }

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

    void _deferRemaining(const std::vector<Position> &positions, size_t from, int64_t bucketTick);

    static constexpr size_t MAX_UPDATES_PER_TICK = 4096;

    std::map<int64_t, std::vector<Position>> mBuckets;
    std::unordered_map<int64_t, std::vector<Position>> mParked;
    std::unordered_map<Position, int64_t, PositionHash> mSchedule;
    int64_t mTick = 0;
    size_t mLastProcessed = 0;
};
