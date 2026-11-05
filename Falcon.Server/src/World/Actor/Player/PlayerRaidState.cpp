#include "World/Actor/Player/PlayerRaidState.h"

#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Village/Raid.h"
#include "World/Village/Village.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/OnScreenTextureAnimationPacket.h"

#include <cmath>

namespace {
    // player.json: gain_raid_omen adds 30 seconds of raid_omen with the on screen animation and removes bad_omen;
    // no amplifier is given, so the bad omen level is not carried over
    const int32_t RAID_OMEN_DURATION_TICKS = 600;

    // RaidBossComponent, set up in sub_141022F80 and named in sub_142CD7520
    const char *const BAR_NAME = "raid.name";
    const char *const BAR_VICTORY_TITLE = "%raid.name - %raid.victory";
    const char *const BAR_DEFEAT_TITLE = "%raid.name - %raid.defeat";
    const char *const BAR_REMAINING_TITLE = "%raid.name - %raid.progress ";
    const size_t BAR_REMAINING_MAX = 2;
    // BossBarColor::RED and BossBarOverlay::PROGRESS (sub_142CD92E0)
    const int32_t BAR_COLOR = 2;
    const int32_t BAR_OVERLAY = 0;
    // the client is offered the bar at most once per second of real time (sub_142CD8F00)
    const std::chrono::seconds BAR_CHECK_INTERVAL(1);

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

void PlayerRaidState::onRaidOmenAdded(const Vector3i &position) {
    if (mHasOmenPosition)
        return;

    mHasOmenPosition = true;
    mOmenPosition = position;
}

void PlayerRaidState::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned())
        return;

    _tickBadOmen(owner, player);
    _tickTrigger(owner, player);
    _tickBossBar(owner, player);
}

void PlayerRaidState::_tickBadOmen(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.hasEffect(MobEffectId::BadOmen))
        return;

    VillageManager &villages = owner.getLevelFor(player).getVillageManager();
    if (villages.getClosestVillage(floored(player.getPosition()), 0) == nullptr)
        return;

    player.removeEffect(MobEffectId::BadOmen);
    owner.addEffect(player, MobEffectId::RaidOmen, 0, RAID_OMEN_DURATION_TICKS);

    OnScreenTextureAnimationPacket animation;
    animation.mEffectId = (uint32_t) MobEffectId::RaidOmen;
    owner.sendPacketTo(player.getNetworkIdentifier(), animation);
}

void PlayerRaidState::_tickTrigger(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!mTriggerPending)
        return;

    // the omen position is used once, then the player's own position until a village takes the raid (sub_147737BD0,
    // then the legacy trigger sub_14CE891B0): player.json only drops raid_trigger through its triggered_event
    const Vector3i position = mHasOmenPosition ? mOmenPosition : floored(player.getPosition());
    mHasOmenPosition = false;

    const std::shared_ptr<Village> village =
            owner.getLevelFor(player).getVillageManager().getClosestVillage(position, 0);
    if (village == nullptr || village->getRaid() != nullptr)
        return;

    village->tryStartRaid(owner);
    mTriggerPending = false;
}

void PlayerRaidState::_tickBossBar(ServerNetworkHandler &owner, ServerPlayer &player) {
    // RaidBossSystem, sub_1477374B0: the bar stays with its village until the raid ends or a closer raid takes over,
    // even out of range, and its name and fill are sent every tick (sub_142CD7210)
    VillageManager &villages = owner.getLevelFor(player).getVillageManager();
    const std::shared_ptr<Village> closest = villages.getClosestRaidingVillage(floored(player.getPosition()));
    if (closest != nullptr && closest != mBarVillage.lock()) {
        mBarActive = false;
        mBarPercent = 0.0f;
        _sendBossEvent(owner, player, BossEventPacket::Action::UpdatePercentage);
        mBarVillage.reset();
        _sendBossEvent(owner, player, BossEventPacket::Action::Remove);
        mBarVillage = closest;
        mBarDimension = player.getDimension();
    }

    const std::shared_ptr<Village> village = mBarVillage.lock();
    const Raid *raid = village == nullptr ? nullptr : village->getRaid();
    if (raid == nullptr) {
        if (mBarActive)
            _hideBossBar(owner, player);
        return;
    }

    mBarActive = true;
    _refreshBossBarRegistration(owner, player, *village);

    // between waves and while a wave is looked for or spawned, the bar keeps its last title and fill
    switch (raid->getState()) {
        case Raid::State::Ended:
            break;
        case Raid::State::Ongoing:
            mBarShowsRemaining = true;
            break;
        case Raid::State::Cooldown:
            mBarShowsRemaining = false;
            break;
        default:
            return;
    }

    _sendBossEvent(owner, player, BossEventPacket::Action::UpdateName);
    mBarPercent = raid->getProgress(owner);
    _sendBossEvent(owner, player, BossEventPacket::Action::UpdatePercentage);
}

void PlayerRaidState::_hideBossBar(ServerNetworkHandler &owner, ServerPlayer &player) {
    mBarActive = false;
    mBarPercent = 0.0f;
    _sendBossEvent(owner, player, BossEventPacket::Action::UpdatePercentage);
    mBarVillage.reset();
    _sendBossEvent(owner, player, BossEventPacket::Action::Remove);
}

void PlayerRaidState::_refreshBossBarRegistration(ServerNetworkHandler &owner, ServerPlayer &player,
                                                  const Village &village) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (mLastBarCheck + BAR_CHECK_INTERVAL >= now)
        return;
    mLastBarCheck = now;

    // the bar is only offered while the player stands in the raid area it was shown for, in its dimension
    // (sub_142CD90D0)
    if (mBarSent || player.getDimension() != mBarDimension || !village.isInRaidBounds(player.getPosition()))
        return;

    mBarSent = true;
    _sendBossEvent(owner, player, BossEventPacket::Action::Create);
}

void PlayerRaidState::_sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player,
                                     BossEventPacket::Action action) {
    if (!mBarSent)
        return;

    BossEventPacket packet;
    packet.mBossUniqueActorId = player.getUniqueId();
    packet.mAction = action;
    packet.mTitle = _getBossBarTitle();
    packet.mHealthPercentage = mBarPercent;
    packet.mColor = BAR_COLOR;
    packet.mOverlay = BAR_OVERLAY;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);

    if (action == BossEventPacket::Action::Remove)
        mBarSent = false;
}

std::string PlayerRaidState::_getBossBarTitle() const {
    const std::shared_ptr<Village> village = mBarVillage.lock();
    const Raid *raid = village == nullptr ? nullptr : village->getRaid();
    if (raid == nullptr)
        return BAR_NAME;

    if (raid->getState() == Raid::State::Ended)
        return raid->getResult() == Raid::Result::Victory ? BAR_VICTORY_TITLE : BAR_DEFEAT_TITLE;

    const size_t remaining = raid->getRaiders().size();
    if (remaining > BAR_REMAINING_MAX || !mBarShowsRemaining)
        return BAR_NAME;

    // the count is appended as a single digit character
    return std::string(BAR_REMAINING_TITLE) + (char) ('0' | (int32_t) remaining);
}
