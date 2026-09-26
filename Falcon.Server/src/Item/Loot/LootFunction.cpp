#include "Item/Loot/LootFunction.h"

#include "Item/EnchantmentData.h"
#include "Item/EnchantmentHelper.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Item/PotionEffects.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
    const int32_t GEAR_BASE_LEVELS = 5;
    const int32_t GEAR_EXTRA_LEVELS = 18;
    const int32_t OMINOUS_BANNER_TYPE = 1;
    const int32_t OMINOUS_BANNER_DATA = 15;
    const char *EMPTY_MAP = "minecraft:empty_map";

    uint64_t randomSeed(const LootContext &context) {
        return ((uint64_t) context.mRandom() << 32) | (uint64_t) context.mRandom();
    }

    std::string withoutNamespace(const std::string &name) {
        const size_t separator = name.find(':');
        return separator == std::string::npos ? name : name.substr(separator + 1);
    }

    const std::unordered_map<std::string, std::string> &smeltingResults() {
        static const std::unordered_map<std::string, std::string> results = {
                {"minecraft:beef", "minecraft:cooked_beef"},
                {"minecraft:chicken", "minecraft:cooked_chicken"},
                {"minecraft:porkchop", "minecraft:cooked_porkchop"},
                {"minecraft:mutton", "minecraft:cooked_mutton"},
                {"minecraft:rabbit", "minecraft:cooked_rabbit"},
                {"minecraft:cod", "minecraft:cooked_cod"},
                {"minecraft:salmon", "minecraft:cooked_salmon"},
                {"minecraft:potato", "minecraft:baked_potato"}
        };
        return results;
    }
}

LootFunction::LootFunction(const json::Value &definition)
        : mConditions(LootCondition::createAll(definition.get("conditions"))) {
}

void LootFunction::run(LootDrop &drop, const LootContext &context) const {
    if (LootCondition::testAll(mConditions, context))
        apply(drop, context);
}

std::unique_ptr<LootFunction> LootFunction::create(const json::Value &definition) {
    const json::Value *type = definition.get("function");
    if (type == nullptr)
        return nullptr;

    const std::string name = withoutNamespace(type->string());
    if (name == "set_count")
        return std::make_unique<SetCountFunction>(definition);
    if (name == "set_data" || name == "random_aux_value")
        return std::make_unique<SetDataFunction>(definition);
    if (name == "set_damage")
        return std::make_unique<SetDamageFunction>(definition);
    if (name == "looting_enchant")
        return std::make_unique<LootingEnchantFunction>(definition);
    if (name == "furnace_smelt")
        return std::make_unique<FurnaceSmeltFunction>(definition);
    if (name == "enchant_with_levels")
        return std::make_unique<EnchantWithLevelsFunction>(definition);
    if (name == "enchant_randomly")
        return std::make_unique<EnchantRandomlyFunction>(definition);
    if (name == "enchant_random_gear")
        return std::make_unique<EnchantRandomGearFunction>(definition);
    if (name == "set_banner_details")
        return std::make_unique<SetBannerDetailsFunction>(definition);
    if (name == "set_data_from_color_index")
        return std::make_unique<SetDataFromColorIndexFunction>(definition);
    if (name == "exploration_map")
        return std::make_unique<ExplorationMapFunction>(definition);
    if (name == "specific_enchants")
        return std::make_unique<SpecificEnchantsFunction>(definition);
    if (name == "set_potion")
        return std::make_unique<SetPotionFunction>(definition);

    return nullptr;
}

std::vector<std::unique_ptr<LootFunction>> LootFunction::createAll(const json::Value *definitions) {
    std::vector<std::unique_ptr<LootFunction>> functions;
    if (definitions == nullptr || !definitions->isArray())
        return functions;

    for (const std::unique_ptr<json::Value> &definition: definitions->mArray) {
        std::unique_ptr<LootFunction> function = create(*definition);
        if (function != nullptr)
            functions.push_back(std::move(function));
    }

    return functions;
}

SetCountFunction::SetCountFunction(const json::Value &definition)
        : LootFunction(definition), mCount(LootRange::parse(definition.get("count"), 1.0f)) {
}

void SetCountFunction::apply(LootDrop &drop, const LootContext &context) const {
    drop.mCount = mCount.rollInt(context.mRandom);
}

SetDataFunction::SetDataFunction(const json::Value &definition)
        : LootFunction(definition),
          mData(LootRange::parse(definition.get("data") != nullptr ? definition.get("data") : definition.get("values"),
                                 0.0f)) {
}

void SetDataFunction::apply(LootDrop &drop, const LootContext &context) const {
    drop.mData = mData.rollInt(context.mRandom);
}

SetDamageFunction::SetDamageFunction(const json::Value &definition)
        : LootFunction(definition), mDamage(LootRange::parse(definition.get("damage"), 1.0f)) {
}

void SetDamageFunction::apply(LootDrop &drop, const LootContext &context) const {
    drop.mDurabilityFraction = std::clamp(mDamage.rollFloat(context.mRandom), 0.0f, 1.0f);
}

LootingEnchantFunction::LootingEnchantFunction(const json::Value &definition)
        : LootFunction(definition), mCount(LootRange::parse(definition.get("count"), 0.0f)) {
}

void LootingEnchantFunction::apply(LootDrop &drop, const LootContext &context) const {
    if (context.mLootingLevel <= 0)
        return;

    drop.mCount += (int32_t) std::lround(mCount.rollFloat(context.mRandom) * (float) context.mLootingLevel);
}

FurnaceSmeltFunction::FurnaceSmeltFunction(const json::Value &definition) : LootFunction(definition) {
}

void FurnaceSmeltFunction::apply(LootDrop &drop, const LootContext &context) const {
    (void) context;

    const auto result = smeltingResults().find(drop.mIdentifier);
    if (result != smeltingResults().end())
        drop.mIdentifier = result->second;
}

EnchantWithLevelsFunction::EnchantWithLevelsFunction(const json::Value &definition)
        : LootFunction(definition), mLevels(LootRange::parse(definition.get("levels"), 1.0f)),
          mTreasure(definition.get("treasure") != nullptr && definition.get("treasure")->boolean(false)) {
}

void EnchantWithLevelsFunction::apply(LootDrop &drop, const LootContext &context) const {
    const std::string identifier = LegacyItemMapper::getInstance().resolve(drop.mIdentifier, drop.mData);
    drop.mEnchantments = EnchantmentHelper::enchantWithLevels(identifier, mLevels.rollInt(context.mRandom), mTreasure,
                                                             randomSeed(context));
}

EnchantRandomlyFunction::EnchantRandomlyFunction(const json::Value &definition)
        : LootFunction(definition),
          mTreasure(definition.get("treasure") != nullptr && definition.get("treasure")->boolean(false)) {
}

void EnchantRandomlyFunction::apply(LootDrop &drop, const LootContext &context) const {
    const std::string identifier = LegacyItemMapper::getInstance().resolve(drop.mIdentifier, drop.mData);
    drop.mEnchantments = EnchantmentHelper::enchantRandomly(identifier, mTreasure, randomSeed(context));
}

EnchantRandomGearFunction::EnchantRandomGearFunction(const json::Value &definition)
        : LootFunction(definition),
          mChance(definition.get("chance") == nullptr ? 1.0f : (float) definition.get("chance")->number(1.0)) {
}

void EnchantRandomGearFunction::apply(LootDrop &drop, const LootContext &context) const {
    const float regional = std::clamp(context.mRegionalDifficulty, 0.0f, 1.0f);
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(context.mRandom) >= mChance * regional)
        return;

    const int32_t levels = GEAR_BASE_LEVELS
                           + (int32_t) (regional * (float) std::uniform_int_distribution<int32_t>(
                                   0, GEAR_EXTRA_LEVELS - 1)(context.mRandom));
    const std::string identifier = LegacyItemMapper::getInstance().resolve(drop.mIdentifier, drop.mData);
    drop.mEnchantments = EnchantmentHelper::enchantWithLevels(identifier, levels, false, randomSeed(context));
}

SetBannerDetailsFunction::SetBannerDetailsFunction(const json::Value &definition)
        : LootFunction(definition),
          mType(definition.get("type") == nullptr ? 0 : definition.get("type")->integer(0)) {
}

void SetBannerDetailsFunction::apply(LootDrop &drop, const LootContext &context) const {
    (void) context;

    drop.mExtraData.putInt("Type", mType);
    if (mType == OMINOUS_BANNER_TYPE)
        drop.mData = OMINOUS_BANNER_DATA;
}

SetDataFromColorIndexFunction::SetDataFromColorIndexFunction(const json::Value &definition)
        : LootFunction(definition) {
}

void SetDataFromColorIndexFunction::apply(LootDrop &drop, const LootContext &context) const {
    drop.mData = context.mColorIndex;
}

SpecificEnchantsFunction::SpecificEnchantsFunction(const json::Value &definition) : LootFunction(definition) {
    const json::Value *enchants = definition.get("enchants");
    if (enchants == nullptr || !enchants->isArray())
        return;

    for (const std::unique_ptr<json::Value> &enchant: enchants->mArray) {
        const json::Value *id = enchant->isString() ? enchant.get() : enchant->get("id");
        const EnchantmentData *data = id == nullptr ? nullptr : EnchantmentTable::findByName(id->string());
        if (data == nullptr)
            continue;

        mEnchants.push_back(Entry{data->mId, LootRange::parse(enchant->isString() ? nullptr : enchant->get("level"),
                                                              1.0f)});
    }
}

void SpecificEnchantsFunction::apply(LootDrop &drop, const LootContext &context) const {
    for (const Entry &entry: mEnchants)
        drop.mEnchantments.push_back(EnchantmentInstance{entry.mId, std::max(1, entry.mLevel.rollInt(context.mRandom))});
}

SetPotionFunction::SetPotionFunction(const json::Value &definition)
        : LootFunction(definition),
          mPotionId(definition.get("id") == nullptr ? -1 : findPotionId(definition.get("id")->string())) {
}

void SetPotionFunction::apply(LootDrop &drop, const LootContext &context) const {
    (void) context;
    if (mPotionId >= 0)
        drop.mData = mPotionId;
}

ExplorationMapFunction::ExplorationMapFunction(const json::Value &definition) : LootFunction(definition) {
}

void ExplorationMapFunction::apply(LootDrop &drop, const LootContext &context) const {
    (void) context;

    drop.mIdentifier = EMPTY_MAP;
    drop.mData = 0;
}
