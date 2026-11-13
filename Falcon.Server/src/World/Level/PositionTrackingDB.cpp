#include "World/Level/PositionTrackingDB.h"

#include "World/Level/Level.h"

#include <cstdio>
#include <cstdlib>

namespace {
    const char *const LAST_ID_KEY = "PositionTrackDB-LastId";
    const char *const TAG_VERSION = "version";
    const char *const TAG_ID = "id";
    const char *const TAG_POSITION = "pos";
    const char *const TAG_DIMENSION = "dim";
    const char *const TAG_STATUS = "status";
    const int8_t VERSION = 1;

    // "0x" then eight lowercase hex digits
    std::string formatId(int32_t id) {
        char text[16];
        std::snprintf(text, sizeof(text), "0x%08x", (uint32_t) id);
        return text;
    }

    int32_t parseId(const std::string &text) {
        return (int32_t) std::strtoull(text.c_str(), nullptr, 0);
    }

    std::string recordKey(int32_t id) {
        return "PosTrackDB-" + formatId(id);
    }
}

void PositionTrackingDB::load(Level &storage) {
    mStorage = &storage;
    mRecords.clear();
    mLastId = INVALID_ID;

    Tag saved;
    if (!storage.loadSavedData(LAST_ID_KEY, saved, false) || saved.getByte(TAG_VERSION, 0) != VERSION)
        return;
    const std::string text = saved.getString(TAG_ID);
    if (!text.empty())
        mLastId = parseId(text);
}

const TrackingRecord &PositionTrackingDB::track(const Vector3i &position, int32_t dimension) {
    int32_t id = mLastId + 1;
    if (id == INVALID_ID)
        ++id;
    mLastId = id;

    if (mStorage != nullptr) {
        Tag last = Tag::ofCompound();
        last.putByte(TAG_VERSION, VERSION);
        last.putString(TAG_ID, formatId(id));
        mStorage->saveSavedData(LAST_ID_KEY, last, false);
    }

    TrackingRecord &record = mRecords[id];
    record.mId = id;
    record.mPosition = position;
    record.mDimension = dimension;
    record.mStatus = TrackingRecordStatus::Valid;
    _save(record);
    return record;
}

const TrackingRecord *PositionTrackingDB::find(int32_t id) {
    if (id == INVALID_ID)
        return nullptr;

    const auto cached = mRecords.find(id);
    if (cached != mRecords.end())
        return &cached->second;

    Tag saved;
    if (mStorage == nullptr || !mStorage->loadSavedData(recordKey(id), saved, false))
        return nullptr;

    const Tag *position = saved.get(TAG_POSITION);
    if (saved.getByte(TAG_VERSION, 0) != VERSION || position == nullptr || !position->isList()
        || position->getList().size() != 3)
        return nullptr;

    const std::vector<Tag> &coordinates = position->getList();
    TrackingRecord &record = mRecords[id];
    record.mId = id;
    record.mPosition = Vector3i(coordinates[0].asInt(), coordinates[1].asInt(), coordinates[2].asInt());
    record.mDimension = saved.getInt(TAG_DIMENSION, 0);
    record.mStatus = (TrackingRecordStatus) saved.getByte(TAG_STATUS, 0);
    return &record;
}

const TrackingRecord *PositionTrackingDB::destroy(int32_t id) {
    if (find(id) == nullptr)
        return nullptr;

    TrackingRecord &record = mRecords[id];
    record.mStatus = TrackingRecordStatus::Destroyed;
    if (mStorage != nullptr)
        mStorage->eraseSavedData(recordKey(id));
    return &record;
}

Tag PositionTrackingDB::toTag(const TrackingRecord &record) {
    Tag tag = Tag::ofCompound();
    tag.putByte(TAG_VERSION, VERSION);
    tag.putString(TAG_ID, formatId(record.mId));
    tag.put(TAG_POSITION, Tag::ofList(Tag::Type::Int, {Tag::ofInt(record.mPosition.x), Tag::ofInt(record.mPosition.y),
                                                       Tag::ofInt(record.mPosition.z)}));
    tag.putInt(TAG_DIMENSION, record.mDimension);
    tag.putByte(TAG_STATUS, (int8_t) record.mStatus);
    return tag;
}

void PositionTrackingDB::_save(const TrackingRecord &record) {
    if (mStorage != nullptr)
        mStorage->saveSavedData(recordKey(record.mId), toTag(record), false);
}
