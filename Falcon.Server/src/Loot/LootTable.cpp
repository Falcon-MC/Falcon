#include "Loot/LootTable.h"

#include "Loot/LegacyItemMapper.h"
#include "Loot/LootTableRegistry.h"

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
}

LootPool::LootPool(const json::Value &definition)
        : mRolls(LootRange::parse(definition.get("rolls"), 1.0f)),
          mConditions(LootCondition::createAll(definition.get("conditions"))) {
    const json::Value *entries = definition.get("entries");
    if (entries == nullptr || !entries->isArray())
        return;

    for (const std::unique_ptr<json::Value> &entry: entries->mArray)
        mEntries.emplace_back(*entry);
}

void LootPool::roll(const LootContext &context, std::vector<LootDrop> &drops, int32_t depth) const {
    if (!LootCondition::testAll(mConditions, context))
        return;

    const int32_t rolls = mRolls.rollInt(context.mRandom);
    for (int32_t roll = 0; roll < rolls; ++roll) {
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
