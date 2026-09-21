#pragma once

#include "Core/NBT/Tag.h"
#include "Item/ItemEnchantments.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct LootContext {
    explicit LootContext(std::mt19937 &random) : mRandom(random) {
    }

    std::mt19937 &mRandom;
    int32_t mLootingLevel = 0;
    int32_t mDifficulty = 2;
    float mRegionalDifficulty = 0.0f;
    bool mKilledByPlayer = false;
    bool mKilledByPet = false;
    bool mOnFire = false;
    int32_t mMarkVariant = 0;
    int32_t mColorIndex = 0;
    std::string mKillerIdentifier;
};

struct LootDrop {
    std::string mIdentifier;
    int32_t mData = 0;
    int32_t mCount = 1;
    float mDurabilityFraction = 1.0f;
    std::vector<EnchantmentInstance> mEnchantments;
    Tag mExtraData = Tag::ofCompound();
};
