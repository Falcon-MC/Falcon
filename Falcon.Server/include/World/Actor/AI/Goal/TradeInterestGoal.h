#pragma once

#include "Protocol/Types/ItemStack.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>

class ServerPlayer;
struct MerchantOffer;

// minecraft:behavior.trade_interest: a trader holds up what it would give for the item a nearby player holds
class TradeInterestGoal final : public Goal {
public:
    struct Settings {
        float mRadius = 0.0f;
        int32_t mInterestTicks = 0;
        int32_t mRemoveItemTicks = 0;
        int32_t mSwitchTicks = 0;
        int32_t mCooldownTicks = 0;
    };

    explicit TradeInterestGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    // the next offer after the one shown that takes what the player holds
    const MerchantOffer *_nextOffer(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &held);

    ServerPlayer *_player(ServerNetworkHandler &owner) const;

    void _show(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item);

    Settings mSettings;
    int64_t mPlayerId = 0;
    ItemStack mShown;
    size_t mOfferIndex = 0;
    int64_t mNextUseTick = 0;
    int32_t mInterestTicks = 0;
    int32_t mEmptyHandTicks = 0;
    int32_t mShownTicks = 0;
};
