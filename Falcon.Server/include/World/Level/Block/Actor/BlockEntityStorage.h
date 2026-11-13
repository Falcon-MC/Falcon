#pragma once

#include "Core/NBT/Tag.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// entities kept inside a block, one bucket per script module; the block actor saves them as BlockEntityStorage
class BlockEntityStorage {
public:
    // a store or transfer is refused once the buckets together hold four
    static constexpr size_t CAPACITY = 4;

    // the codes the game turns into messages
    enum class Result : uint8_t {
        Ok = 0,
        Full = 2,
        InvalidId = 3,
        AlreadyStored = 4,
        SerializeFailed = 5,
        NotFound = 6,
        DeserializeFailed = 7,
        SpawnFailed = 8,
        SameStorage = 9,
        NoTargetStorage = 10,
        NotStorable = 11,
        Riding = 12,
    };

    static const char *message(Result result);

    size_t total() const;

    bool contains(int64_t uniqueId) const;

    const std::vector<Tag> *bucket(const std::string &key) const;

    // the saved actor, its UniqueID and Definition already written
    void add(const std::string &key, Tag entity);

    // the stored entity leaves its bucket, an emptied bucket goes too
    bool take(const std::string &key, int64_t uniqueId, Tag &entity);

    // buckets without entities are left out, no bucket at all removes the tag
    void save(Tag &data) const;

    void load(const Tag &data);

    static int64_t uniqueIdOf(const Tag &entity);

private:
    std::map<std::string, std::vector<Tag>> mBuckets;
};
