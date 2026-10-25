#pragma once

#include "Core/Json/Json.h"
#include "World/Level/Storage/Loot/LootCondition.h"
#include "World/Level/Storage/Loot/LootContext.h"
#include "World/Level/Storage/Loot/LootFunction.h"
#include "World/Level/Storage/Loot/LootRange.h"

#include <memory>
#include <string>
#include <vector>

class LootPool;

class LootEntry {
public:
    enum class Type {
        Item,
        Table,
        Empty
    };

    explicit LootEntry(const json::Value &definition);

    /** Luck shifts the weight by quality per point, as fishing loot uses for Luck of the Sea. */
    int32_t getWeight(const LootContext &context) const;

    bool isAvailable(const LootContext &context) const;

    void roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const;

private:
    Type mType = Type::Empty;
    std::string mName;
    int32_t mWeight = 1;
    int32_t mQuality = 0;
    std::vector<std::unique_ptr<LootCondition>> mConditions;
    std::vector<std::unique_ptr<LootFunction>> mFunctions;
    std::vector<LootPool> mPools;
};

class LootPool {
public:
    explicit LootPool(const json::Value &definition);

    void roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const;

private:
    void _rollTier(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const;

    LootRange mRolls;
    std::vector<std::unique_ptr<LootCondition>> mConditions;
    std::vector<LootEntry> mEntries;
    bool mHasTiers = false;
    int32_t mTierInitialRange = 1;
    int32_t mTierBonusRolls = 0;
    float mTierBonusChance = 0.0f;
};

class LootTable {
public:
    static constexpr int32_t MAX_NESTING = 8;

    explicit LootTable(const json::Value &definition);

    std::vector<LootDrop> roll(const LootContext &context) const;

    void roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const;

private:
    std::vector<LootPool> mPools;
};
