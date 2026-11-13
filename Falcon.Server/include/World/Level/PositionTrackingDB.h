#pragma once

#include "Core/NBT/Tag.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>
#include <unordered_map>

class Level;

enum class TrackingRecordStatus : uint8_t {
    Valid = 0,
    Pending = 1,
    Destroyed = 2
};

struct TrackingRecord {
    int32_t mId = 0;
    Vector3i mPosition;
    int32_t mDimension = 0;
    TrackingRecordStatus mStatus = TrackingRecordStatus::Valid;
};

// PositionTrackingDB: lodestone positions by tracking id, level-wide under PosTrackDB-0x%08x; main thread only
class PositionTrackingDB {
public:
    // 0 never names a record
    static const int32_t INVALID_ID = 0;

    // the last id handed out, from PositionTrackDB-LastId
    void load(Level &storage);

    // a new id for the position, saved with the last id
    const TrackingRecord &track(const Vector3i &position, int32_t dimension);

    // the cached record, else the stored one; nullptr when neither exists
    const TrackingRecord *find(int32_t id);

    // the Destroy operation: marked destroyed and erased from storage
    const TrackingRecord *destroy(int32_t id);

    static Tag toTag(const TrackingRecord &record);

private:
    void _save(const TrackingRecord &record);

    Level *mStorage = nullptr;
    int32_t mLastId = INVALID_ID;
    std::unordered_map<int32_t, TrackingRecord> mRecords;
};
