#include "World/Actor/Trade/MerchantOffers.h"

#include "World/Inventory/ItemStackNbt.h"
#include "World/Inventory/PlayerInventory.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    const char *TAG_RECIPES = "Recipes";
    const char *TAG_TIER_EXP_REQUIREMENTS = "TierExpRequirements";
    const char *TAG_BUY_A = "buyA";
    const char *TAG_BUY_B = "buyB";
    const char *TAG_SELL = "sell";
    const char *TAG_TIER = "tier";
    const char *TAG_USES = "uses";
    const char *TAG_MAX_USES = "maxUses";
    const char *TAG_TRADER_EXP = "traderExp";
    const char *TAG_REWARD_EXP = "rewardExp";
    const char *TAG_DEMAND = "demand";
    const char *TAG_BUY_COUNT_A = "buyCountA";
    const char *TAG_BUY_COUNT_B = "buyCountB";
    const char *TAG_PRICE_MULTIPLIER_A = "priceMultiplierA";
    const char *TAG_PRICE_MULTIPLIER_B = "priceMultiplierB";
    const char *TAG_NET_ID = "netId";

    const float HERO_BASE_DISCOUNT = 0.3f;
    const float HERO_DISCOUNT_PER_LEVEL = 0.0625f;
    // the high tier cure counts five times the low one
    const int32_t HIGH_CURE_WEIGHT = 5;

    int32_t sNextNetId = 0;

    int32_t intOr(const Tag &data, const char *key, int32_t fallback) {
        const Tag *value = data.get(key);
        return value != nullptr && value->getType() == Tag::Type::Int ? value->asInt() : fallback;
    }

    float floatOr(const Tag &data, const char *key, float fallback) {
        const Tag *value = data.get(key);
        return value != nullptr && value->getType() == Tag::Type::Float ? value->asFloat() : fallback;
    }

    ItemStack itemOf(const Tag &data, const char *key, const PacketCodecContext &context) {
        const Tag *item = data.get(key);
        return item != nullptr && item->isCompound() ? ItemStackNbt::read(*item, context) : ItemStack::air();
    }
}

void MerchantOffer::updatePrice(int32_t lowCureDiscount, int32_t highCureDiscount, bool hero, int32_t heroAmplifier) {
    if (mBuyA.isAir())
        return;

    int32_t heroDiscount = 0;
    if (hero) {
        const float share = (float) mBuyCountA * ((float) heroAmplifier * HERO_DISCOUNT_PER_LEVEL + HERO_BASE_DISCOUNT);
        heroDiscount = -std::max(1, (int32_t) std::floor(share));
    }

    const int32_t cure = lowCureDiscount + HIGH_CURE_WEIGHT * highCureDiscount;
    const int32_t demand = std::max(0, (int32_t) std::floor((float) (mBuyCountA * mDemand) * mPriceMultiplierA));
    const int32_t price = demand + (int32_t) (mPriceMultiplierA * (float) cure) + heroDiscount + mBuyCountA;
    mBuyA.mCount = std::min(std::max(1, price), PlayerInventory::getMaxStackSize(mBuyA));
}

void MerchantOffer::resupply() {
    mDemand = std::max(0, mDemand + 2 * mUses - mMaxUses);
    mUses = 0;
    updatePrice(0, 0, false, 0);
}

MerchantOffers MerchantOffers::load(const Tag &data, const PacketCodecContext &context) {
    MerchantOffers offers;
    int32_t tierCount = 0;
    const Tag *recipes = data.get(TAG_RECIPES);
    if (recipes != nullptr && recipes->isList()) {
        for (const Tag &recipe: recipes->getList()) {
            if (!recipe.isCompound())
                continue;

            MerchantOffer offer;
            offer.mBuyA = itemOf(recipe, TAG_BUY_A, context);
            offer.mSell = itemOf(recipe, TAG_SELL, context);
            offer.mBuyB = itemOf(recipe, TAG_BUY_B, context);
            offer.mTier = intOr(recipe, TAG_TIER, 0);
            offer.mUses = intOr(recipe, TAG_USES, 0);
            offer.mMaxUses = intOr(recipe, TAG_MAX_USES, 7);
            offer.mTraderExp = intOr(recipe, TAG_TRADER_EXP, 0);
            const Tag *reward = recipe.get(TAG_REWARD_EXP);
            offer.mRewardExp = reward == nullptr || reward->getType() != Tag::Type::Byte || reward->asByte() != 0;
            offer.mDemand = intOr(recipe, TAG_DEMAND, 0);
            offer.mBuyCountA = intOr(recipe, TAG_BUY_COUNT_A, offer.mBuyA.isAir() ? 0 : offer.mBuyA.mCount);
            offer.mBuyCountB = intOr(recipe, TAG_BUY_COUNT_B, offer.mBuyB.isAir() ? 0 : offer.mBuyB.mCount);
            offer.mPriceMultiplierA = floatOr(recipe, TAG_PRICE_MULTIPLIER_A, 0.0f);
            offer.mPriceMultiplierB = floatOr(recipe, TAG_PRICE_MULTIPLIER_B, 0.0f);
            offer.mNetId = intOr(recipe, TAG_NET_ID, 0);
            tierCount = std::max(tierCount, offer.mTier + 1);
            offers.mOffers.push_back(std::move(offer));
        }
    }

    // only as many requirements as the offers have tiers
    const Tag *requirements = data.get(TAG_TIER_EXP_REQUIREMENTS);
    if (requirements != nullptr && requirements->isList()) {
        const std::vector<Tag> &entries = requirements->getList();
        const size_t count = std::min(entries.size(), (size_t) tierCount);
        for (size_t index = 0; index < count; ++index) {
            const Tag &entry = entries[index];
            offers.mTierExpRequirements.push_back(
                    entry.isCompound() ? intOr(entry, std::to_string(index).c_str(), 0) : 0);
        }
    }

    for (MerchantOffer &offer: offers.mOffers) {
        if (offer.mNetId == 0)
            offer.mNetId = nextNetId();
    }
    return offers;
}

Tag MerchantOffers::save(bool withNetIds) const {
    Tag data = Tag::ofCompound();
    Tag recipes = Tag::ofList(Tag::Type::Compound);
    for (const MerchantOffer &offer: mOffers) {
        Tag recipe = Tag::ofCompound();
        recipe.put(TAG_BUY_A, ItemStackNbt::write(offer.mBuyA));
        recipe.put(TAG_SELL, ItemStackNbt::write(offer.mSell));
        if (!offer.mBuyB.isAir())
            recipe.put(TAG_BUY_B, ItemStackNbt::write(offer.mBuyB));
        recipe.putInt(TAG_TIER, offer.mTier);
        recipe.putInt(TAG_USES, offer.mUses);
        recipe.putInt(TAG_MAX_USES, offer.mMaxUses);
        recipe.putInt(TAG_TRADER_EXP, offer.mTraderExp);
        recipe.putBool(TAG_REWARD_EXP, offer.mRewardExp);
        recipe.putInt(TAG_DEMAND, offer.mDemand);
        recipe.putInt(TAG_BUY_COUNT_A, offer.mBuyCountA);
        recipe.putInt(TAG_BUY_COUNT_B, offer.mBuyCountB);
        recipe.putFloat(TAG_PRICE_MULTIPLIER_A, offer.mPriceMultiplierA);
        recipe.putFloat(TAG_PRICE_MULTIPLIER_B, offer.mPriceMultiplierB);
        if (withNetIds)
            recipe.putInt(TAG_NET_ID, offer.mNetId);
        recipes.addToList(std::move(recipe));
    }
    data.put(TAG_RECIPES, std::move(recipes));

    Tag requirements = Tag::ofList(Tag::Type::Compound);
    for (size_t index = 0; index < mTierExpRequirements.size(); ++index) {
        Tag entry = Tag::ofCompound();
        entry.putInt(std::to_string(index), mTierExpRequirements[index]);
        requirements.addToList(std::move(entry));
    }
    data.put(TAG_TIER_EXP_REQUIREMENTS, std::move(requirements));
    return data;
}

int32_t MerchantOffers::nextNetId() {
    // zero means no id: the counter skips it when it wraps
    if (++sNextNetId == 0)
        sNextNetId = 1;
    return sNextNetId;
}

MerchantOffer *MerchantOffers::findByNetId(int32_t netId) {
    const int32_t index = indexOfNetId(netId);
    return index < 0 ? nullptr : &mOffers[(size_t) index];
}

int32_t MerchantOffers::indexOfNetId(int32_t netId) const {
    if (netId == 0)
        return -1;
    for (size_t index = 0; index < mOffers.size(); ++index) {
        if (mOffers[index].mNetId == netId)
            return (int32_t) index;
    }
    return -1;
}

int32_t MerchantOffers::tierForExperience(int32_t experience) const {
    size_t reached = 0;
    while (reached < mTierExpRequirements.size() && experience >= mTierExpRequirements[reached])
        ++reached;
    return std::max<int32_t>(1, (int32_t) reached) - 1;
}
