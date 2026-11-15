#include "World/Actor/Mob/Component/MobEconomy.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Trade/TradeTableRegistry.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/EntityDataMap.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {
    const char *TAG_RICHES = "Riches";
    const char *TAG_OFFERS = "Offers";
    const char *TAG_CONVERTED_FROM_VILLAGER_V1 = "ConvertedFromVillagerV1";
    const char *TAG_TRADE_TABLE_PATH = "TradeTablePath";
    const char *TAG_LOW_TIER_CURED_DISCOUNT = "LowTierCuredDiscount";
    const char *TAG_HIGH_TIER_CURED_DISCOUNT = "HighTierCuredDiscount";
    const char *TAG_NEARBY_CURED_DISCOUNT = "NearbyCuredDiscount";
    const char *TAG_NEARBY_CURED_DISCOUNT_TIME_STAMP = "NearbyCuredDiscountTimeStamp";
    const char *TAG_TRADE_TIER = "TradeTier";
    const char *TAG_TRADE_EXPERIENCE = "TradeExperience";

    using Trades = std::vector<std::shared_ptr<const TradeEntry>>;

    std::vector<const TradeItemChoice *> passingChoices(ServerNetworkHandler &owner, const MobActor &mob,
                                                       const TradeItemEntry &entry) {
        std::vector<const TradeItemChoice *> choices;
        for (const TradeItemChoice &choice: entry.mChoices) {
            if (choice.mFilters == nullptr || EntityFilter::test(*choice.mFilters, owner, mob))
                choices.push_back(&choice);
        }
        return choices;
    }

    // the game keeps counts within a stack: the range is clamped first, its maximum stays out of the roll
    bool rollCount(ServerNetworkHandler &owner, const TradeItemChoice &choice, LootDrop &drop) {
        drop.mIdentifier = choice.mIdentifier;
        drop.mData = choice.mData;
        drop.mCount = 1;
        const ItemStack probe = LootItems::toItemStack(owner, drop);
        if (probe.isAir())
            return false;

        const int32_t maxStack = PlayerInventory::getMaxStackSize(probe);
        const int32_t minimum = std::min(choice.mMinCount, maxStack);
        const int32_t maximum = std::min(choice.mMaxCount, maxStack);
        drop.mCount = maximum > minimum ? minimum + GameRandom::nextInt(MobEconomy::random(), maximum - minimum)
                                       : minimum;
        return true;
    }

    int32_t applyFunctions(const TradeItemChoice &choice, LootDrop &drop) {
        const LootContext context(MobEconomy::random());
        int32_t price = 0;
        for (const std::unique_ptr<TradeItemFunction> &function: choice.mFunctions) {
            const int32_t result = function->apply(drop, context);
            if (result > 0)
                price = result;
        }
        return price;
    }

    const TradeItemChoice *pick(const std::vector<const TradeItemChoice *> &choices) {
        return choices[(size_t) GameRandom::nextInt(MobEconomy::random(), (int32_t) choices.size())];
    }

    bool rollOffer(ServerNetworkHandler &owner, const MobActor &mob, const TradeEntry &trade, int32_t tier,
                   MerchantOffer &offer) {
        const std::vector<const TradeItemChoice *> wantsA = passingChoices(owner, mob, trade.mWants[0]);
        const std::vector<const TradeItemChoice *> gives = passingChoices(owner, mob, trade.mGives[0]);
        if (wantsA.empty() || gives.empty())
            return false;
        const std::vector<const TradeItemChoice *> wantsB =
                trade.mWants.size() >= 2 ? passingChoices(owner, mob, trade.mWants[1])
                                         : std::vector<const TradeItemChoice *>();

        const TradeItemChoice &choiceA = *pick(wantsA);
        LootDrop buyA;
        if (!rollCount(owner, choiceA, buyA))
            return false;

        const TradeItemChoice *choiceB = wantsB.empty() ? nullptr : pick(wantsB);
        LootDrop buyB;
        if (choiceB != nullptr && !rollCount(owner, *choiceB, buyB))
            choiceB = nullptr;

        const TradeItemChoice &choiceSell = *pick(gives);
        LootDrop sell;
        if (!rollCount(owner, choiceSell, sell))
            return false;

        applyFunctions(choiceA, buyA);
        if (choiceB != nullptr)
            applyFunctions(*choiceB, buyB);
        const int32_t price = applyFunctions(choiceSell, sell);

        offer.mBuyA = LootItems::toItemStack(owner, buyA);
        offer.mBuyB = choiceB == nullptr ? ItemStack::air() : LootItems::toItemStack(owner, buyB);
        offer.mSell = LootItems::toItemStack(owner, sell);
        if (offer.mBuyA.isAir() || offer.mSell.isAir())
            return false;

        // an enchanted book sets its own price
        if (price > 0)
            offer.mBuyA.mCount = std::min(price, PlayerInventory::getMaxStackSize(offer.mBuyA));

        offer.mTier = tier;
        offer.mMaxUses = trade.mMaxUses;
        offer.mTraderExp = trade.mTraderExp;
        offer.mRewardExp = trade.mRewardExp;
        offer.mPriceMultiplierA = choiceA.mPriceMultiplier;
        offer.mPriceMultiplierB = choiceB == nullptr ? 0.0f : choiceB->mPriceMultiplier;
        offer.mBuyCountA = offer.mBuyA.mCount;
        offer.mBuyCountB = offer.mBuyB.isAir() ? 0 : offer.mBuyB.mCount;
        offer.mNetId = MerchantOffers::nextNetId();
        return true;
    }

    // weighted draws without replacement into the front, then those are shuffled
    void selectTrades(Trades &trades, int32_t count) {
        std::mt19937 &random = MobEconomy::random();
        int32_t totalWeight = 0;
        for (const std::shared_ptr<const TradeEntry> &trade: trades)
            totalWeight += trade->mWeight;

        for (int32_t slot = 0; slot < count; ++slot) {
            int32_t roll = GameRandom::nextInt(random, totalWeight);
            for (size_t index = (size_t) slot; index < trades.size(); ++index) {
                roll -= trades[index]->mWeight;
                if (roll >= 0)
                    continue;
                totalWeight -= trades[index]->mWeight;
                std::swap(trades[(size_t) slot], trades[index]);
                break;
            }
        }

        for (int32_t index = 1; index < count; ++index) {
            const int32_t other = GameRandom::nextInt(random, index + 1);
            if (other != index)
                std::swap(trades[(size_t) index], trades[(size_t) other]);
        }
    }
}

bool MobEconomy::isTrader(const MobActor &mob) {
    return mob.getComponent(COMPONENT) != nullptr;
}

std::mt19937 &MobEconomy::random() {
    static std::mt19937 generator(std::random_device{}());
    return generator;
}

MerchantOffers *MobEconomy::getOffers(ServerNetworkHandler &owner, MobActor &mob) {
    if (mHasSavedOffers) {
        mOffers = std::make_unique<MerchantOffers>(MerchantOffers::load(mSavedOffers, owner.getCodecContext()));
        mSavedOffers = Tag();
        mHasSavedOffers = false;
        if (mResupplyPending)
            resupply();
    }

    const json::Value *component = mob.getComponent(COMPONENT);
    const json::Value *table = component == nullptr ? nullptr : component->get("table");
    const std::string path = table == nullptr ? std::string() : table->string();
    if (path.empty())
        return mOffers.get();

    if (mOffers != nullptr && (mTablePath == path || mTablePath.empty())) {
        mTablePath = path;
        // the cap is not saved: a loaded trader reads it back from its table
        const TradeTable *loaded = TradeTableRegistry::getInstance().get(path);
        if (loaded != nullptr && !loaded->getTiers().empty())
            mMaxTier = (int32_t) loaded->getTiers().size() - 1;
        return mOffers.get();
    }

    _generate(owner, mob, path);
    return mOffers.get();
}

void MobEconomy::_generate(ServerNetworkHandler &owner, MobActor &mob, const std::string &path) {
    const TradeTable *table = TradeTableRegistry::getInstance().get(path);
    if (table == nullptr)
        return;

    auto offers = std::make_unique<MerchantOffers>();
    const std::vector<TradeTier> &tiers = table->getTiers();
    mMaxTier = tiers.empty() ? 0 : (int32_t) tiers.size() - 1;
    mTier = std::min(mTier, mMaxTier);

    // every tier at once: the screen shows the locked ones
    for (size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex) {
        const TradeTier &tier = tiers[tierIndex];
        offers->mTierExpRequirements.push_back(tier.mTotalExpRequired);

        for (const TradeGroup &group: tier.mGroups) {
            Trades trades = group.mTrades;
            int32_t count = group.mNumToSelect;
            if (count <= 0)
                count = (int32_t) trades.size();
            else if ((int32_t) trades.size() > count)
                selectTrades(trades, count);

            // a trade whose items all fail their filters gives its place to the next one
            int32_t produced = 0;
            for (const std::shared_ptr<const TradeEntry> &trade: trades) {
                if (produced >= count)
                    break;
                MerchantOffer offer;
                if (!rollOffer(owner, mob, *trade, (int32_t) tierIndex, offer))
                    continue;
                offers->mOffers.push_back(std::move(offer));
                ++produced;
            }
        }
    }

    mOffers = std::move(offers);
    mTablePath = path;
}

void MobEconomy::resupply() {
    if (mHasSavedOffers) {
        mResupplyPending = true;
        return;
    }
    mResupplyPending = false;
    if (mOffers == nullptr)
        return;
    for (MerchantOffer &offer: mOffers->mOffers)
        offer.resupply();
}

void MobEconomy::syncTier() {
    const int32_t capped = std::min(mTier, mMaxTier);
    const std::vector<int32_t> *requirements = mOffers == nullptr ? nullptr : &mOffers->mTierExpRequirements;
    const int32_t required = requirements != nullptr && capped >= 0 && (size_t) capped < requirements->size()
                                     ? (*requirements)[(size_t) capped]
                                     : 0;
    if (mExperience < required) {
        mTier = capped;
        mExperience = required;
        return;
    }
    mTier = mOffers == nullptr ? 0 : mOffers->tierForExperience(mExperience);
}

void MobEconomy::appendMetadata(EntityDataMap &metadata) const {
    const auto pushInt = [&metadata](int32_t id, int32_t value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = value;
        metadata.mEntries.push_back(entry);
    };

    pushInt(TRADE_TIER_DATA_ID, mTier);
    pushInt(MAX_TRADE_TIER_DATA_ID, mMaxTier);
    pushInt(TRADE_EXPERIENCE_DATA_ID, mExperience);
    pushInt(LOW_TIER_CURED_DISCOUNT_DATA_ID, mLowTierCuredDiscount);
    pushInt(HIGH_TIER_CURED_DISCOUNT_DATA_ID, mHighTierCuredDiscount);
    pushInt(NEARBY_CURED_DISCOUNT_DATA_ID, mNearbyCuredDiscount);
    pushInt(NEARBY_CURED_DISCOUNT_TIME_STAMP_DATA_ID, mNearbyCuredDiscountTimeStamp);
}

void MobEconomy::saveNbt(Tag &data) const {
    data.putInt(TAG_RICHES, mRiches);
    if (mOffers != nullptr)
        data.put(TAG_OFFERS, mOffers->save(false));
    else if (mHasSavedOffers)
        data.put(TAG_OFFERS, mSavedOffers);
    if (mConvertedFromVillagerV1)
        data.putBool(TAG_CONVERTED_FROM_VILLAGER_V1, true);
    if (!mTablePath.empty())
        data.putString(TAG_TRADE_TABLE_PATH, mTablePath);
    data.putInt(TAG_LOW_TIER_CURED_DISCOUNT, mLowTierCuredDiscount);
    data.putInt(TAG_HIGH_TIER_CURED_DISCOUNT, mHighTierCuredDiscount);
    data.putInt(TAG_NEARBY_CURED_DISCOUNT, mNearbyCuredDiscount);
    data.putInt(TAG_NEARBY_CURED_DISCOUNT_TIME_STAMP, mNearbyCuredDiscountTimeStamp);
    data.putInt(TAG_TRADE_TIER, mTier);
    data.putInt(TAG_TRADE_EXPERIENCE, mExperience);
}

void MobEconomy::loadNbt(const Tag &data) {
    mRiches = data.getInt(TAG_RICHES, 0);
    const Tag *offers = data.get(TAG_OFFERS);
    mHasSavedOffers = offers != nullptr && offers->isCompound();
    mSavedOffers = mHasSavedOffers ? *offers : Tag();
    mOffers.reset();
    mConvertedFromVillagerV1 = data.getByte(TAG_CONVERTED_FROM_VILLAGER_V1, 0) != 0;
    mTablePath = data.getString(TAG_TRADE_TABLE_PATH, std::string());
    mLowTierCuredDiscount = data.getInt(TAG_LOW_TIER_CURED_DISCOUNT, 0);
    mHighTierCuredDiscount = data.getInt(TAG_HIGH_TIER_CURED_DISCOUNT, 0);
    mNearbyCuredDiscount = data.getInt(TAG_NEARBY_CURED_DISCOUNT, 0);
    mNearbyCuredDiscountTimeStamp = data.getInt(TAG_NEARBY_CURED_DISCOUNT_TIME_STAMP, 0);
    mTier = data.getInt(TAG_TRADE_TIER, 0);
    mExperience = data.getInt(TAG_TRADE_EXPERIENCE, 0);
}
