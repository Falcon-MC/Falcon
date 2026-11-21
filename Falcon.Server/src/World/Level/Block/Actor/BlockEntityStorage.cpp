#include "World/Level/Block/Actor/BlockEntityStorage.h"

#include <utility>

namespace {
    const char *const TAG_STORAGE = "BlockEntityStorage";
    const char *const TAG_BUCKET = "Bucket";
    const char *const TAG_ENTITIES = "Entities";
    const char *const TAG_UNIQUE_ID = "UniqueID";
}

const char *BlockEntityStorage::message(Result result) {
    switch (result) {
        case Result::Full:
            return "Entity storage is full.";
        case Result::InvalidId:
            return "Invalid entity ID.";
        case Result::AlreadyStored:
            return "The entity is already stored.";
        case Result::SerializeFailed:
            return "Failed to serialize the entity.";
        case Result::NotFound:
            return "The entity was not found in storage.";
        case Result::DeserializeFailed:
            return "Failed to deserialize the entity.";
        case Result::SpawnFailed:
            return "Failed to spawn the released entity.";
        case Result::SameStorage:
            return "The target block is the same entity storage.";
        case Result::NoTargetStorage:
            return "The target block does not have entity storage.";
        case Result::NotStorable:
            return "Only non-player mobs can be stored.";
        case Result::Riding:
            return "Entities with passengers or that are passengers cannot be stored.";
        case Result::Ok:
            break;
    }
    return "";
}

// a stored entity without UniqueID reads as 0, as the game compares it
int64_t BlockEntityStorage::uniqueIdOf(const Tag &entity) {
    return entity.getLong(TAG_UNIQUE_ID, 0);
}

size_t BlockEntityStorage::total() const {
    size_t count = 0;
    for (const auto &bucket: mBuckets)
        count += bucket.second.size();
    return count;
}

bool BlockEntityStorage::contains(int64_t uniqueId) const {
    for (const auto &bucket: mBuckets) {
        for (const Tag &entity: bucket.second) {
            if (uniqueIdOf(entity) == uniqueId)
                return true;
        }
    }
    return false;
}

const std::vector<Tag> *BlockEntityStorage::bucket(const std::string &key) const {
    const auto found = mBuckets.find(key);
    return found == mBuckets.end() ? nullptr : &found->second;
}

void BlockEntityStorage::add(const std::string &key, Tag entity) {
    mBuckets[key].push_back(std::move(entity));
}

bool BlockEntityStorage::take(const std::string &key, int64_t uniqueId, Tag &entity) {
    const auto found = mBuckets.find(key);
    if (found == mBuckets.end())
        return false;
    std::vector<Tag> &entities = found->second;
    for (auto it = entities.begin(); it != entities.end(); ++it) {
        if (uniqueIdOf(*it) != uniqueId)
            continue;
        entity = std::move(*it);
        entities.erase(it);
        if (entities.empty())
            mBuckets.erase(found);
        return true;
    }
    return false;
}

void BlockEntityStorage::save(Tag &data) const {
    if (mBuckets.empty()) {
        data.remove(TAG_STORAGE);
        return;
    }
    std::vector<Tag> buckets;
    for (const auto &bucket: mBuckets) {
        if (bucket.second.empty())
            continue;
        Tag entry = Tag::ofCompound();
        entry.putString(TAG_BUCKET, bucket.first);
        entry.put(TAG_ENTITIES, Tag::ofList(Tag::Type::Compound, bucket.second));
        buckets.push_back(std::move(entry));
    }
    data.put(TAG_STORAGE, Tag::ofList(Tag::Type::Compound, std::move(buckets)));
}

void BlockEntityStorage::load(const Tag &data) {
    mBuckets.clear();
    const Tag *buckets = data.get(TAG_STORAGE);
    if (buckets == nullptr || !buckets->isList())
        return;
    for (const Tag &entry: buckets->getList()) {
        if (!entry.isCompound())
            continue;
        const Tag *entities = entry.get(TAG_ENTITIES);
        if (entities == nullptr || !entities->isList())
            continue;
        std::vector<Tag> &stored = mBuckets[entry.getString(TAG_BUCKET)];
        for (const Tag &entity: entities->getList()) {
            if (entity.isCompound())
                stored.push_back(entity);
        }
        if (stored.empty())
            mBuckets.erase(entry.getString(TAG_BUCKET));
    }
}
