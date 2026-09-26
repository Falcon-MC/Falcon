#pragma once

#include "Core/Json/Json.h"
#include "Item/Loot/LootCondition.h"
#include "Item/Loot/LootContext.h"
#include "Item/Loot/LootRange.h"

#include <memory>
#include <vector>

class LootFunction {
public:
    explicit LootFunction(const json::Value &definition);

    virtual ~LootFunction() = default;

    void run(LootDrop &drop, const LootContext &context) const;

    static std::unique_ptr<LootFunction> create(const json::Value &definition);

    static std::vector<std::unique_ptr<LootFunction>> createAll(const json::Value *definitions);

protected:
    virtual void apply(LootDrop &drop, const LootContext &context) const = 0;

private:
    std::vector<std::unique_ptr<LootCondition>> mConditions;
};

class SetCountFunction final : public LootFunction {
public:
    explicit SetCountFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    LootRange mCount;
};

class SetDataFunction final : public LootFunction {
public:
    explicit SetDataFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    LootRange mData;
};

class SetDamageFunction final : public LootFunction {
public:
    explicit SetDamageFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    LootRange mDamage;
};

class LootingEnchantFunction final : public LootFunction {
public:
    explicit LootingEnchantFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    LootRange mCount;
};

class FurnaceSmeltFunction final : public LootFunction {
public:
    explicit FurnaceSmeltFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;
};

class EnchantWithLevelsFunction final : public LootFunction {
public:
    explicit EnchantWithLevelsFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    LootRange mLevels;
    bool mTreasure;
};

class EnchantRandomlyFunction final : public LootFunction {
public:
    explicit EnchantRandomlyFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    bool mTreasure;
};

class EnchantRandomGearFunction final : public LootFunction {
public:
    explicit EnchantRandomGearFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    float mChance;
};

class SetBannerDetailsFunction final : public LootFunction {
public:
    explicit SetBannerDetailsFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;

private:
    int32_t mType;
};

class SetDataFromColorIndexFunction final : public LootFunction {
public:
    explicit SetDataFromColorIndexFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;
};

class ExplorationMapFunction final : public LootFunction {
public:
    explicit ExplorationMapFunction(const json::Value &definition);

protected:
    void apply(LootDrop &drop, const LootContext &context) const override;
};
