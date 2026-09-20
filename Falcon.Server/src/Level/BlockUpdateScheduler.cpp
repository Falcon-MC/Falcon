#include "Level/BlockUpdateScheduler.h"

#include "Level/LevelChunk.h"

#include <algorithm>
#include <utility>

void BlockUpdateScheduler::moveStateFrom(BlockUpdateScheduler &&other) {
    if (this == &other)
        return;

    mTick = other.mTick;
    mBuckets = std::move(other.mBuckets);
    mParked = std::move(other.mParked);
    mSchedule = std::move(other.mSchedule);
    mLastProcessed = other.mLastProcessed;
}

void BlockUpdateScheduler::schedule(const Vector3i &position, int64_t delay) {
    if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y)
        return;

    const Position key{position.x, position.y, position.z};
    const int64_t due = mTick + std::max<int64_t>(1, delay);

    auto it = mSchedule.find(key);
    if (it != mSchedule.end() && due >= it->second)
        return;

    mSchedule[key] = due;
    mBuckets[due].push_back(key);
}

void BlockUpdateScheduler::cancel(const Vector3i &position) {
    mSchedule.erase(Position{position.x, position.y, position.z});
}

bool BlockUpdateScheduler::isScheduled(const Vector3i &position) const {
    return mSchedule.find(Position{position.x, position.y, position.z}) != mSchedule.end();
}

void BlockUpdateScheduler::park(int32_t chunkX, int32_t chunkZ, const Vector3i &position) {
    const int64_t column = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    mParked[column].push_back(Position{position.x, position.y, position.z});
}

void BlockUpdateScheduler::activateColumn(int32_t chunkX, int32_t chunkZ) {
    const int64_t column = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;

    auto it = mParked.find(column);
    if (it == mParked.end())
        return;

    std::vector<Position> parked;
    parked.swap(it->second);
    mParked.erase(it);

    for (const Position &position: parked)
        schedule(Vector3i(position.x, position.y, position.z), 1);
}

void BlockUpdateScheduler::_deferRemaining(const std::vector<Position> &positions, size_t from,
                                           int64_t bucketTick) {
    if (from >= positions.size())
        return;

    std::vector<Position> &target = mBuckets[bucketTick];
    target.insert(target.end(), positions.begin() + (long) from, positions.end());
}

void BlockUpdateScheduler::tick(const UpdateHandler &handler,
                                const std::function<bool(int32_t, int32_t)> &isActive) {
    ++mTick;
    mLastProcessed = 0;

    if (mBuckets.empty())
        return;

    bool exhausted = false;

    while (!mBuckets.empty() && !exhausted) {
        auto bucket = mBuckets.begin();
        if (bucket->first > mTick)
            break;

        std::vector<Position> positions;
        positions.swap(bucket->second);
        const int64_t bucketTick = bucket->first;
        mBuckets.erase(bucket);

        for (size_t i = 0; i < positions.size(); ++i) {
            const Position &key = positions[i];

            auto scheduled = mSchedule.find(key);
            if (scheduled == mSchedule.end() || scheduled->second != bucketTick)
                continue;

            mSchedule.erase(scheduled);

            if (!isActive(key.x >> 4, key.z >> 4)) {
                park(key.x >> 4, key.z >> 4, Vector3i(key.x, key.y, key.z));
                continue;
            }

            handler(Vector3i(key.x, key.y, key.z));
            mLastProcessed++;

            if (mLastProcessed >= MAX_UPDATES_PER_TICK) {
                _deferRemaining(positions, i + 1, bucketTick);
                exhausted = true;
                break;
            }
        }
    }
}
