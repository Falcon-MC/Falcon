#include "Item/Loot/LootTable.h"

#include "Item/Loot/LegacyItemMapper.h"
#include "Item/Loot/LootTableRegistry.h"

#include <algorithm>
#include <random>

LootEntry::LootEntry(const json::Value &definition)
        : mConditions(LootCondition::createAll(definition.get("conditions"))),
          mFunctions(LootFunction::createAll(definition.get("functions"))) {
    const json::Value *type = definition.get("type");
    const std::string typeName = type == nullptr ? std::string() : type->string();
    if (typeName == "item")
        mType = Type::Item;
    else if (typeName == "loot_table")
        mType = Type::Table;

    const json::Value *name = definition.get("name");
    if (name != nullptr)
        mName = name->string();

    const json::Value *weight = definition.get("weight");
    if (weight != nullptr)
        mWeight = weight->integer(1);

    const json::Value *pools = definition.get("pools");
    if (pools == nullptr || !pools->isArray())
        return;

    for (const std::unique_ptr<json::Value> &pool: pools->mArray)
        mPools.emplace_back(*pool);
}

bool LootEntry::isAvailable(const LootContext &context) const {
    return LootCondition::testAll(mConditions, context);
}

void LootEntry::roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const {
    if (mType == Type::Table) {
        const LootTable *table = LootTableRegistry::getInstance().get(mName);
        if (table != nullptr && depth < LootTable::MAX_NESTING)
            table->roll(context, drops, depth + 1);
        return;
    }

    if (mType != Type::Item || mName.empty())
        return;

    LootDrop drop;
    drop.mIdentifier = mName;
    for (const std::unique_ptr<LootFunction> &function: mFunctions)
        function->run(drop, context);

    const std::string named = drop.mIdentifier.find(':') == std::string::npos ? "minecraft:" + drop.mIdentifier
                                                                               : drop.mIdentifier;
    const std::string resolved = LegacyItemMapper::getInstance().resolve(named, drop.mData);
    if (resolved != named)
        drop.mData = 0;
    drop.mIdentifier = resolved;

    if (drop.mCount > 0)
        drops.push_back(drop);

    if (depth >= LootTable::MAX_NESTING)
        return;

    for (const LootPool &pool: mPools)
        pool.roll(context, drops, depth + 1);
}

LootPool::LootPool(const json::Value &definition)
        : mRolls(LootRange::parse(definition.get("rolls"), 1.0f)),
          mConditions(LootCondition::createAll(definition.get("conditions"))) {
    const json::Value *entries = definition.get("entries");
    if (entries == nullptr || !entries->isArray())
        return;

    for (const std::unique_ptr<json::Value> &entry: entries->mArray)
        mEntries.emplace_back(*entry);

    const json::Value *tiers = definition.get("tiers");
    if (tiers == nullptr || !tiers->isObject())
        return;

    mHasTiers = true;
    const json::Value *initialRange = tiers->get("initial_range");
    if (initialRange != nullptr)
        mTierInitialRange = std::max(1, initialRange->integer(1));

    const json::Value *bonusRolls = tiers->get("bonus_rolls");
    if (bonusRolls != nullptr)
        mTierBonusRolls = std::max(0, bonusRolls->integer(0));

    const json::Value *bonusChance = tiers->get("bonus_chance");
    if (bonusChance != nullptr)
        mTierBonusChance = (float) bonusChance->number(0.0);
}

void LootPool::_rollTier(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const {
    if (mEntries.empty())
        return;

    int32_t tier = std::uniform_int_distribution<int32_t>(0, mTierInitialRange - 1)(context.mRandom);
    for (int32_t bonus = 0; bonus < mTierBonusRolls; ++bonus) {
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(context.mRandom) < mTierBonusChance)
            tier++;
    }

    const LootEntry &entry = mEntries[(size_t) std::min(tier, (int32_t) mEntries.size() - 1)];
    if (entry.isAvailable(context))
        entry.roll(context, drops, depth);
}

void LootPool::roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const {
    if (!LootCondition::testAll(mConditions, context))
        return;

    const int32_t rolls = mRolls.rollInt(context.mRandom);
    for (int32_t roll = 0; roll < rolls; ++roll) {
        if (mHasTiers) {
            _rollTier(context, drops, depth);
            continue;
        }

        std::vector<const LootEntry *> available;
        int32_t totalWeight = 0;
        for (const LootEntry &entry: mEntries) {
            if (entry.getWeight() > 0 && entry.isAvailable(context)) {
                available.push_back(&entry);
                totalWeight += entry.getWeight();
            }
        }

        if (totalWeight <= 0)
            return;

        int32_t pick = std::uniform_int_distribution<int32_t>(0, totalWeight - 1)(context.mRandom);
        for (const LootEntry *entry: available) {
            if (pick < entry->getWeight()) {
                entry->roll(context, drops, depth);
                break;
            }
            pick -= entry->getWeight();
        }
    }
}

LootTable::LootTable(const json::Value &definition) {
    const json::Value *pools = definition.get("pools");
    if (pools == nullptr || !pools->isArray())
        return;

    for (const std::unique_ptr<json::Value> &pool: pools->mArray)
        mPools.emplace_back(*pool);
}

std::vector<LootDrop> LootTable::roll(const LootContext &context) const {
    std::vector<LootDrop> drops;
    roll(context, drops, 0);
    return drops;
}

void LootTable::roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const {
    for (const LootPool &pool: mPools)
        pool.roll(context, drops, depth);
}
