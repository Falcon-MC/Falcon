#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/BlockStateHasher.h"

#include <atomic>
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

    // packed LiquidProperties, 0 until computed; generation threads read it too
    uint32_t getCachedLiquidProperties() const {
        return mLiquidProperties.mValue.load(std::memory_order_relaxed);
    }

    void cacheLiquidProperties(uint32_t properties) const {
        mLiquidProperties.mValue.store(properties, std::memory_order_relaxed);
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

    // a block the server does not know: played as this state, saved back as the tag it was read from
    const Tag *getPreserved() const {
        return mPreserved.mTag;
    }

    void preserve(const Tag *stored) {
        mPreserved.mTag = stored;
    }

    bool operator==(const BlockState &other) const {
        return mName == other.mName && mStates == other.mStates && mPreserved.mTag == other.mPreserved.mTag;
    }

    Tag toNbt() const {
        if (mPreserved.mTag != nullptr)
            return *mPreserved.mTag;
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
    // not copied: a copy may get its name changed in place and would point at the wrong block
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
    // not copied, for the same reason as ResolvedBlock; a race only computes the same value twice
    struct LiquidCache {
        LiquidCache() = default;

        LiquidCache(const LiquidCache &) {}

        LiquidCache &operator=(const LiquidCache &) {
            mValue.store(0, std::memory_order_relaxed);
            return *this;
        }

        std::atomic<uint32_t> mValue{0};
    };

    // not copied: a copy may be renamed in place and must not save as the stored tag
    struct PreservedTag {
        PreservedTag() = default;

        PreservedTag(const PreservedTag &) {}

        PreservedTag &operator=(const PreservedTag &) {
            mTag = nullptr;
            return *this;
        }

        const Tag *mTag = nullptr;
    };

    mutable int32_t mLightProperties = -1;
    mutable ResolvedBlock mResolved;
    PreservedTag mPreserved;
    mutable LiquidCache mLiquidProperties;
};
