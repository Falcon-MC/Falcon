#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/BlockStateHasher.h"

#include <cstdint>
#include <string>

class BlockState {
public:
    BlockState() : mName("minecraft:air"), mStates(Tag::ofCompound()) {}

    explicit BlockState(const std::string &name) : mName(name), mStates(Tag::ofCompound()) {}

    BlockState(const std::string &name, const Tag &states) : mName(name), mStates(states) {}

    int32_t getHash() const {
        if (!mHashValid) {
            mHash = BlockStateHasher::hash(mName, mStates);
            mHashValid = true;
        }
        return mHash;
    }

    int32_t getCachedLightProperties() const {
        return mLightProperties;
    }

    void cacheLightProperties(int32_t properties) const {
        mLightProperties = properties;
    }

    bool operator==(const BlockState &other) const {
        return mName == other.mName && mStates == other.mStates;
    }

    Tag toNbt() const {
        Tag tag = Tag::ofCompound();
        tag.putString("name", mName);
        tag.put("states", mStates);
        tag.putInt("version", BLOCK_STATE_VERSION);
        return tag;
    }

    static const int32_t BLOCK_STATE_VERSION = 18168865;

    std::string mName;
    Tag mStates;

private:
    mutable int32_t mHash = 0;
    mutable bool mHashValid = false;
    mutable int32_t mLightProperties = -1;
};
