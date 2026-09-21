#include "Loot/LootCondition.h"

#include <algorithm>
#include <random>

namespace {
    bool chance(std::mt19937 &random, float probability) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < probability;
    }

    std::string withoutNamespace(const std::string &name) {
        const size_t separator = name.find(':');
        return separator == std::string::npos ? name : name.substr(separator + 1);
    }
}

std::unique_ptr<LootCondition> LootCondition::create(const json::Value &definition) {
    const json::Value *type = definition.get("condition");
    if (type == nullptr)
        return nullptr;

    const std::string name = withoutNamespace(type->string());
    if (name == "random_chance")
        return std::make_unique<RandomChanceCondition>(definition);
    if (name == "random_chance_with_looting")
        return std::make_unique<RandomChanceWithLootingCondition>(definition);
    if (name == "random_difficulty_chance")
        return std::make_unique<RandomDifficultyChanceCondition>(definition);
    if (name == "random_regional_difficulty_chance")
        return std::make_unique<RandomRegionalDifficultyChanceCondition>(definition);
    if (name == "killed_by_player")
        return std::make_unique<KilledByPlayerCondition>(false);
    if (name == "killed_by_player_or_pets")
        return std::make_unique<KilledByPlayerCondition>(true);
    if (name == "killed_by_entity")
        return std::make_unique<KilledByEntityCondition>(definition);
    if (name == "has_mark_variant")
        return std::make_unique<HasMarkVariantCondition>(definition);
    if (name == "entity_properties")
        return std::make_unique<EntityOnFireCondition>(definition);

    return nullptr;
}

std::vector<std::unique_ptr<LootCondition>> LootCondition::createAll(const json::Value *definitions) {
    std::vector<std::unique_ptr<LootCondition>> conditions;
    if (definitions == nullptr || !definitions->isArray())
        return conditions;

    for (const std::unique_ptr<json::Value> &definition: definitions->mArray) {
        std::unique_ptr<LootCondition> condition = create(*definition);
        if (condition != nullptr)
            conditions.push_back(std::move(condition));
    }

    return conditions;
}

bool LootCondition::testAll(const std::vector<std::unique_ptr<LootCondition>> &conditions,
                            const LootContext &context) {
    return std::all_of(conditions.begin(), conditions.end(), [&context](const std::unique_ptr<LootCondition> &condition) {
        return condition->test(context);
    });
}

RandomChanceCondition::RandomChanceCondition(const json::Value &definition)
        : mChance(definition.get("chance") == nullptr ? 1.0f : (float) definition.get("chance")->number(1.0)) {
}

bool RandomChanceCondition::test(const LootContext &context) const {
    return chance(context.mRandom, mChance);
}

RandomChanceWithLootingCondition::RandomChanceWithLootingCondition(const json::Value &definition)
        : mChance(definition.get("chance") == nullptr ? 1.0f : (float) definition.get("chance")->number(1.0)),
          mLootingMultiplier(definition.get("looting_multiplier") == nullptr
                             ? 0.0f
                             : (float) definition.get("looting_multiplier")->number(0.0)) {
}

bool RandomChanceWithLootingCondition::test(const LootContext &context) const {
    return chance(context.mRandom, mChance + mLootingMultiplier * (float) context.mLootingLevel);
}

RandomDifficultyChanceCondition::RandomDifficultyChanceCondition(const json::Value &definition) {
    static const char *const NAMES[] = {"peaceful", "easy", "normal", "hard"};

    const json::Value *fallback = definition.get("default_chance");
    const float defaultChance = fallback == nullptr ? 1.0f : (float) fallback->number(1.0);

    for (int index = 0; index < 4; ++index) {
        const json::Value *value = definition.get(NAMES[index]);
        mChances[index] = value == nullptr ? defaultChance : (float) value->number(defaultChance);
    }
}

bool RandomDifficultyChanceCondition::test(const LootContext &context) const {
    const int index = std::clamp(context.mDifficulty, 0, 3);
    return chance(context.mRandom, mChances[index]);
}

RandomRegionalDifficultyChanceCondition::RandomRegionalDifficultyChanceCondition(const json::Value &definition)
        : mMaxChance(definition.get("max_chance") == nullptr ? 1.0f
                                                              : (float) definition.get("max_chance")->number(1.0)) {
}

bool RandomRegionalDifficultyChanceCondition::test(const LootContext &context) const {
    return chance(context.mRandom, mMaxChance * std::clamp(context.mRegionalDifficulty, 0.0f, 1.0f));
}

KilledByPlayerCondition::KilledByPlayerCondition(bool allowPets) : mAllowPets(allowPets) {
}

bool KilledByPlayerCondition::test(const LootContext &context) const {
    return context.mKilledByPlayer || (mAllowPets && context.mKilledByPet);
}

KilledByEntityCondition::KilledByEntityCondition(const json::Value &definition)
        : mEntityType(definition.get("entity_type") == nullptr ? std::string()
                                                                : definition.get("entity_type")->string()) {
}

bool KilledByEntityCondition::test(const LootContext &context) const {
    return context.mKillerIdentifier == mEntityType;
}

HasMarkVariantCondition::HasMarkVariantCondition(const json::Value &definition)
        : mValue(definition.get("value") == nullptr ? 0 : definition.get("value")->integer(0)) {
}

bool HasMarkVariantCondition::test(const LootContext &context) const {
    return context.mMarkVariant == mValue;
}

EntityOnFireCondition::EntityOnFireCondition(const json::Value &definition) : mOnFire(true) {
    const json::Value *properties = definition.get("properties");
    const json::Value *onFire = properties == nullptr ? nullptr : properties->get("on_fire");
    if (onFire != nullptr)
        mOnFire = onFire->boolean(true);
}

bool EntityOnFireCondition::test(const LootContext &context) const {
    return context.mOnFire == mOnFire;
}
