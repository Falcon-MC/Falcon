#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Level/Storage/Loot/LootContext.h"
#include "World/Level/Storage/Loot/LootFunction.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

class TradeItemFunction {
public:
    virtual ~TradeItemFunction() = default;

    // a positive result is the price the first wanted item takes
    virtual int32_t apply(LootDrop &drop, const LootContext &context) const = 0;

    static std::unique_ptr<TradeItemFunction> create(const json::Value &definition);
};

// one item a trade may want or give; the game rolls its count from [min, max)
struct TradeItemChoice {
    std::string mIdentifier;
    int32_t mData = 0;
    int32_t mMinCount = 1;
    int32_t mMaxCount = 1;
    float mPriceMultiplier = 0.0f;
    std::vector<std::unique_ptr<TradeItemFunction>> mFunctions;
    CompiledEntityFilter mFilters;
};

struct TradeItemEntry {
    std::vector<TradeItemChoice> mChoices;
};

struct TradeEntry {
    std::vector<TradeItemEntry> mWants;
    std::vector<TradeItemEntry> mGives;
    int32_t mTraderExp = 1;
    int32_t mMaxUses = 7;
    bool mRewardExp = true;
    int32_t mWeight = 1;
};

struct TradeGroup {
    // -1: every trade of the group
    int32_t mNumToSelect = -1;
    std::vector<std::shared_ptr<const TradeEntry>> mTrades;
};

struct TradeTier {
    int32_t mTotalExpRequired = 0;
    std::vector<TradeGroup> mGroups;
};

class TradeTable {
public:
    // nullptr when the game refuses the whole table: no tiers, or a group asking for more trades than it has
    static std::unique_ptr<TradeTable> parse(const json::Value &definition);

    const std::vector<TradeTier> &getTiers() const {
        return mTiers;
    }

private:
    std::vector<TradeTier> mTiers;
};
