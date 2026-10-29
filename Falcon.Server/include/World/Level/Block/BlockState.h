#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/BlockStateHasher.h"

#include <cstdint>
#include <string>

class Block;

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

    bool hasResolvedBlock(uint32_t generation) const {
        return mResolved.mGeneration == generation;
    }

    const Block *getResolvedBlock() const {
        return mResolved.mBlock;
    }

    void cacheResolvedBlock(const Block *block, uint32_t generation) const {
        mResolved.mBlock = block;
        mResolved.mGeneration = generation;
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
    /**
     * Not carried over by copies: a copy may have its name changed in place, which would leave it pointing at the
     * wrong block.
     */
    struct ResolvedBlock {
        ResolvedBlock() = default;

        ResolvedBlock(const ResolvedBlock &) {}

        ResolvedBlock &operator=(const ResolvedBlock &) {
            mBlock = nullptr;
            mGeneration = 0;
            return *this;
        }

        const Block *mBlock = nullptr;
        uint32_t mGeneration = 0;
    };

    mutable int32_t mHash = 0;
    mutable bool mHashValid = false;
    mutable int32_t mLightProperties = -1;
    mutable ResolvedBlock mResolved;
};
