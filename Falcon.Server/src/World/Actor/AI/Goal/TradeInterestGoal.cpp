#include "World/Actor/AI/Goal/TradeInterestGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Trade/MerchantOffers.h"
#include "Network/ServerNetworkHandler.h"

#include <cfloat>
#include <utility>

namespace {
    bool takes(const MerchantOffer &offer, const ItemStack &held) {
        return (!offer.mBuyA.isAir() && offer.mBuyA.mDefinition == held.mDefinition)
               || (!offer.mBuyB.isAir() && offer.mBuyB.mDefinition == held.mDefinition);
    }

    bool isOpen(const MerchantOffers &offers, const MerchantOffer &offer) {
        return !offer.isOutOfStock() && offer.mTier <= offers.mUnlockedTier;
    }
}

TradeInterestGoal::TradeInterestGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

ServerPlayer *TradeInterestGoal::_player(ServerNetworkHandler &owner) const {
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getUniqueId() == mPlayerId)
            return entry.second.isSpawned() && entry.second.isAlive() ? &entry.second : nullptr;
    }
    return nullptr;
}

const MerchantOffer *TradeInterestGoal::_nextOffer(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &held) {
    const MerchantOffers *offers = mob.getEconomy().getOffers(owner, mob);
    if (offers == nullptr || offers->mOffers.empty())
        return nullptr;

    const size_t count = offers->mOffers.size();
    for (size_t step = 1; step <= count; ++step) {
        const size_t index = (mOfferIndex + step) % count;
        const MerchantOffer &offer = offers->mOffers[index];
        if (isOpen(*offers, offer) && takes(offer, held)) {
            mOfferIndex = index;
            return &offer;
        }
    }
    return nullptr;
}

void TradeInterestGoal::_show(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    mShown = item;
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, item);
    mob.getEquipment().broadcast(owner, mob);
    mShownTicks = 0;
}

bool TradeInterestGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mNextUseTick)
        return false;
    const MerchantOffers *offers = mob.getEconomy().getOffers(owner, mob);
    if (offers == nullptr)
        return false;

    // the nearest player first
    const float radiusSquared = mSettings.mRadius * mSettings.mRadius;
    ServerPlayer *chosen = nullptr;
    float closest = FLT_MAX;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isAlive() || !player.sharesLevelWith(mob))
            continue;
        const float distance = mob.distanceSquaredTo(player);
        const ItemStack &held = player.getInventory().getItemInHand();
        if (distance > radiusSquared || distance >= closest || held.isAir())
            continue;
        for (const MerchantOffer &offer: offers->mOffers) {
            if (isOpen(*offers, offer) && takes(offer, held)) {
                chosen = &player;
                closest = distance;
                break;
            }
        }
    }
    if (chosen == nullptr)
        return false;

    mPlayerId = chosen->getUniqueId();
    mOfferIndex = offers->mOffers.size() - 1;
    const MerchantOffer *offer = _nextOffer(owner, mob, chosen->getInventory().getItemInHand());
    if (offer == nullptr)
        return false;
    _show(owner, mob, offer->mSell);
    return true;
}

bool TradeInterestGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    ServerPlayer *player = _player(owner);
    if (player == nullptr || mob.distanceSquaredTo(*player) > mSettings.mRadius * mSettings.mRadius
        || mInterestTicks > mSettings.mInterestTicks)
        return false;

    // an empty hand keeps the item up a moment before the interest ends
    const ItemStack &held = player->getInventory().getItemInHand();
    if (held.isAir())
        return ++mEmptyHandTicks <= mSettings.mRemoveItemTicks;
    mEmptyHandTicks = 0;
    ++mInterestTicks;

    if (++mShownTicks < mSettings.mSwitchTicks)
        return true;
    const MerchantOffer *offer = _nextOffer(owner, mob, held);
    if (offer == nullptr)
        return false;
    if (offer->mSell.mDefinition != mShown.mDefinition)
        _show(owner, mob, offer->mSell);
    mShownTicks = 0;
    return true;
}

void TradeInterestGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mInterestTicks = 0;
    mEmptyHandTicks = 0;
    mob.getFlags().set(ActorFlag::TradeInterest, true);
    owner.syncActorFlags(mob);
}

void TradeInterestGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mPlayerId = 0;
    mShown = ItemStack::air();
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, ItemStack::air());
    mob.getEquipment().broadcast(owner, mob);
    mob.getFlags().set(ActorFlag::TradeInterest, false);
    owner.syncActorFlags(mob);
    mNextUseTick = owner.getCurrentTick() + mSettings.mCooldownTicks;
}

void TradeInterestGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (const ServerPlayer *player = _player(owner))
        mob.getLookControl().setLookAt(player->getPosition());
}
