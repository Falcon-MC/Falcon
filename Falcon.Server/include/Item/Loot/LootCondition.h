#pragma once

#include "Core/Json/Json.h"
#include "Item/Loot/LootContext.h"

#include <memory>
#include <string>
#include <vector>

class LootCondition {
public:
    virtual ~LootCondition() = default;

    virtual bool test(const LootContext &context) const = 0;

    static std::unique_ptr<LootCondition> create(const json::Value &definition);

    static std::vector<std::unique_ptr<LootCondition>> createAll(const json::Value *definitions);

    static bool testAll(const std::vector<std::unique_ptr<LootCondition>> &conditions, const LootContext &context);
};

class RandomChanceCondition final : public LootCondition {
public:
    explicit RandomChanceCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    float mChance;
};

class RandomChanceWithLootingCondition final : public LootCondition {
public:
    explicit RandomChanceWithLootingCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    float mChance;
    float mLootingMultiplier;
};

class RandomDifficultyChanceCondition final : public LootCondition {
public:
    explicit RandomDifficultyChanceCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    float mChances[4];
};

class RandomRegionalDifficultyChanceCondition final : public LootCondition {
public:
    explicit RandomRegionalDifficultyChanceCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    float mMaxChance;
};

class KilledByPlayerCondition final : public LootCondition {
public:
    explicit KilledByPlayerCondition(bool allowPets);

    bool test(const LootContext &context) const override;

private:
    bool mAllowPets;
};

class KilledByEntityCondition final : public LootCondition {
public:
    explicit KilledByEntityCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    std::string mEntityType;
};

class HasMarkVariantCondition final : public LootCondition {
public:
    explicit HasMarkVariantCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    int32_t mValue;
};

class EntityOnFireCondition final : public LootCondition {
public:
    explicit EntityOnFireCondition(const json::Value &definition);

    bool test(const LootContext &context) const override;

private:
    bool mOnFire;
};
