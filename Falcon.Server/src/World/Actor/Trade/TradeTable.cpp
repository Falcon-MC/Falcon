#include "World/Actor/Trade/TradeTable.h"

#include "World/Item/EnchantmentData.h"
#include "World/Item/EnchantmentHelper.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/Storage/Loot/LegacyItemMapper.h"

#include <algorithm>
#include <utility>

namespace {
    const int32_t DEFAULT_TRADER_EXP = 1;
    const int32_t DEFAULT_MAX_USES = 7;
    const int32_t MAX_BOOK_COST = 64;

    const char *ENCHANT_BOOK_FOR_TRADING = "enchant_book_for_trading";

    // the enchantments a librarian never sells
    bool isTradeable(int32_t enchantmentId) {
        return enchantmentId != EnchantmentIds::WIND_BURST && enchantmentId != EnchantmentIds::SOUL_SPEED
               && enchantmentId != EnchantmentIds::SWIFT_SNEAK;
    }

    int32_t integerOr(const json::Value *value, int32_t fallback) {
        return value != nullptr && value->isNumber() ? value->integer(fallback) : fallback;
    }

    class LootTradeFunction final : public TradeItemFunction {
    public:
        explicit LootTradeFunction(std::unique_ptr<LootFunction> function) : mFunction(std::move(function)) {}

        int32_t apply(LootDrop &drop, const LootContext &context) const override {
            mFunction->run(drop, context);
            return 0;
        }

    private:
        std::unique_ptr<LootFunction> mFunction;
    };

    class EnchantBookForTradingFunction final : public TradeItemFunction {
    public:
        explicit EnchantBookForTradingFunction(const json::Value &definition)
                : mBaseCost(integerOr(definition.get("base_cost"), 2)),
                  mBaseRandomCost(integerOr(definition.get("base_random_cost"), 5)),
                  mPerLevelRandomCost(integerOr(definition.get("per_level_random_cost"), 10)),
                  mPerLevelCost(integerOr(definition.get("per_level_cost"), 3)) {}

        int32_t apply(LootDrop &drop, const LootContext &context) const override {
            std::vector<const EnchantmentData *> candidates;
            for (size_t index = 0; index < EnchantmentTable::getCount(); ++index) {
                const EnchantmentData &enchantment = EnchantmentTable::at(index);
                if (isTradeable(enchantment.mId))
                    candidates.push_back(&enchantment);
            }
            if (candidates.empty())
                return 0;

            const EnchantmentData &chosen =
                    *candidates[(size_t) GameRandom::nextInt(context.mRandom, (int32_t) candidates.size())];
            const int32_t maxLevel = std::max(1, chosen.mMaxLevel);
            const int32_t level = 1 + GameRandom::nextInt(context.mRandom, maxLevel);

            int32_t cost = GameRandom::nextInt(context.mRandom, mBaseRandomCost + level * mPerLevelRandomCost)
                           + mBaseCost + level * mPerLevelCost;
            if (EnchantmentHelper::isTreasure(chosen.mId))
                cost = std::min(cost * 2, MAX_BOOK_COST);

            drop.mEnchantments.push_back(EnchantmentInstance{chosen.mId, level});
            return cost;
        }

    private:
        int32_t mBaseCost;
        int32_t mBaseRandomCost;
        int32_t mPerLevelRandomCost;
        int32_t mPerLevelCost;
    };

    TradeItemChoice parseChoice(const json::Value &definition) {
        TradeItemChoice choice;
        const json::Value *item = definition.get("item");
        LegacyItemMapper::splitData(item == nullptr ? std::string() : item->string(), choice.mIdentifier,
                                    choice.mData);

        // a missing or broken count is one; the maximum never falls under the minimum
        const json::Value *quantity = definition.get("quantity");
        if (quantity != nullptr && quantity->isNumber()) {
            choice.mMinCount = std::max(1, quantity->integer(1));
            choice.mMaxCount = choice.mMinCount;
        } else if (quantity != nullptr && quantity->isObject()) {
            choice.mMinCount = std::max(1, integerOr(quantity->get("min"), 1));
            choice.mMaxCount = std::max(choice.mMinCount, integerOr(quantity->get("max"), choice.mMinCount));
        }

        const json::Value *multiplier = definition.get("price_multiplier");
        if (multiplier != nullptr && multiplier->isNumber())
            choice.mPriceMultiplier = (float) multiplier->number(0.0);

        const json::Value *functions = definition.get("functions");
        if (functions != nullptr && functions->isArray()) {
            for (const std::unique_ptr<json::Value> &function: functions->mArray) {
                std::unique_ptr<TradeItemFunction> created = TradeItemFunction::create(*function);
                if (created != nullptr)
                    choice.mFunctions.push_back(std::move(created));
            }
        }

        const json::Value *filters = definition.get("filters");
        if (filters != nullptr && filters->isObject())
            choice.mFilters = filters->clone();
        return choice;
    }

    TradeItemEntry parseItem(const json::Value &definition) {
        TradeItemEntry entry;
        const json::Value *choice = definition.get("choice");
        if (choice == nullptr || !choice->isArray()) {
            entry.mChoices.push_back(parseChoice(definition));
            return entry;
        }

        for (const std::unique_ptr<json::Value> &option: choice->mArray) {
            if (option->isObject())
                entry.mChoices.push_back(parseChoice(*option));
        }
        return entry;
    }

    void parseItems(const json::Value *items, std::vector<TradeItemEntry> &target) {
        if (items == nullptr || !items->isArray())
            return;

        for (const std::unique_ptr<json::Value> &item: items->mArray) {
            if (!item->isObject())
                continue;
            TradeItemEntry entry = parseItem(*item);
            if (!entry.mChoices.empty())
                target.push_back(std::move(entry));
        }
    }

    // false when "trades" is not an array; a trade with nothing wanted or given, or a weight under 1, is left out
    bool parseTrades(const json::Value *trades, TradeGroup &group) {
        if (trades == nullptr || !trades->isArray())
            return false;

        for (const std::unique_ptr<json::Value> &definition: trades->mArray) {
            if (!definition->isObject())
                continue;

            auto trade = std::make_shared<TradeEntry>();
            parseItems(definition->get("wants"), trade->mWants);
            if (trade->mWants.empty())
                continue;
            parseItems(definition->get("gives"), trade->mGives);
            if (trade->mGives.empty())
                continue;

            trade->mTraderExp = integerOr(definition->get("trader_exp"), DEFAULT_TRADER_EXP);
            trade->mMaxUses = integerOr(definition->get("max_uses"), DEFAULT_MAX_USES);
            const json::Value *reward = definition->get("reward_exp");
            trade->mRewardExp = reward == nullptr || reward->boolean(false);
            trade->mWeight = integerOr(definition->get("weight"), 1);
            if (trade->mWeight <= 0)
                continue;

            group.mTrades.push_back(std::move(trade));
        }
        return true;
    }
}

std::unique_ptr<TradeItemFunction> TradeItemFunction::create(const json::Value &definition) {
    const json::Value *type = definition.get("function");
    if (type != nullptr && type->string() == ENCHANT_BOOK_FOR_TRADING)
        return std::make_unique<EnchantBookForTradingFunction>(definition);

    std::unique_ptr<LootFunction> loot = LootFunction::create(definition);
    if (loot == nullptr)
        return nullptr;
    return std::make_unique<LootTradeFunction>(std::move(loot));
}

std::unique_ptr<TradeTable> TradeTable::parse(const json::Value &definition) {
    const json::Value *tiers = definition.get("tiers");
    if (tiers == nullptr || !tiers->isArray())
        return nullptr;

    auto table = std::make_unique<TradeTable>();
    for (const std::unique_ptr<json::Value> &definitionTier: tiers->mArray) {
        if (!definitionTier->isObject())
            return nullptr;

        TradeTier tier;
        // a tier without a total takes its own index
        tier.mTotalExpRequired =
                integerOr(definitionTier->get("total_exp_required"), (int32_t) table->mTiers.size());

        const json::Value *groups = definitionTier->get("groups");
        if (groups == nullptr) {
            TradeGroup group;
            if (parseTrades(definitionTier->get("trades"), group))
                tier.mGroups.push_back(std::move(group));
        } else {
            if (!groups->isArray())
                return nullptr;

            for (const std::unique_ptr<json::Value> &definitionGroup: groups->mArray) {
                if (!definitionGroup->isObject())
                    continue;

                TradeGroup group;
                group.mNumToSelect = integerOr(definitionGroup->get("num_to_select"), -1);
                if (!parseTrades(definitionGroup->get("trades"), group))
                    continue;
                if (group.mNumToSelect > (int32_t) group.mTrades.size())
                    return nullptr;
                tier.mGroups.push_back(std::move(group));
            }
        }
        table->mTiers.push_back(std::move(tier));
    }
    return table;
}
