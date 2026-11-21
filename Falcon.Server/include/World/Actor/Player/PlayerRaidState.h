#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Dimension/Dimension.h"
#include "Protocol/Packets/BossEventPacket.h"

#include <chrono>
#include <memory>
#include <string>

class ServerNetworkHandler;
class ServerPlayer;
class Village;

// Bad Omen -> Raid Omen in a village, the raid it starts when removed, the raid boss bar; main thread only
class PlayerRaidState {
public:
    // only the floored position of the first application is kept
    void onRaidOmenAdded(const Vector3i &position);

    // raid asked for on the next tick (minecraft:trigger_raid)
    void onRaidOmenRemoved() {
        mTriggerPending = true;
    }

    void tick(ServerNetworkHandler &owner, ServerPlayer &player);

    bool isBossBarShown() const {
        return mBarSent;
    }

private:
    void _tickBadOmen(ServerNetworkHandler &owner, ServerPlayer &player);

    void _tickTrigger(ServerNetworkHandler &owner, ServerPlayer &player);

    void _tickBossBar(ServerNetworkHandler &owner, ServerPlayer &player);

    void _hideBossBar(ServerNetworkHandler &owner, ServerPlayer &player);

    void _refreshBossBarRegistration(ServerNetworkHandler &owner, ServerPlayer &player, const Village &village);

    void _sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player, BossEventPacket::Action action);

    std::string _getBossBarTitle() const;

    bool mHasOmenPosition = false;
    Vector3i mOmenPosition;
    bool mTriggerPending = false;

    std::weak_ptr<Village> mBarVillage;
    DimensionType mBarDimension = DimensionType::Overworld;
    bool mBarShowsRemaining = false;
    bool mBarActive = false;
    bool mBarSent = false;
    float mBarPercent = 0.0f;
    std::chrono::steady_clock::time_point mLastBarCheck;
};
