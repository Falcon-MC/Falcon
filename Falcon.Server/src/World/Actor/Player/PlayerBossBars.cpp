#include "World/Actor/Player/PlayerBossBars.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

namespace {
    const int32_t BAR_OVERLAY_PROGRESS = 0;

    float clampPercent(float percent) {
        return std::min(1.0f, std::max(0.0f, percent));
    }
}

uint32_t PlayerBossBars::add(const std::string &title, float percent, Color color) {
    const std::lock_guard<std::mutex> lock(mMutex);
    Bar bar;
    bar.mId = mNextId++;
    bar.mTitle = title;
    bar.mPercent = clampPercent(percent);
    bar.mColor = color;
    mBars.push_back(bar);
    return bar.mId;
}

bool PlayerBossBars::update(uint32_t id, const std::string &title, float percent, Color color) {
    const std::lock_guard<std::mutex> lock(mMutex);
    for (Bar &bar: mBars) {
        if (bar.mId != id)
            continue;

        bar.mTitle = title;
        bar.mPercent = clampPercent(percent);
        bar.mColor = color;
        return true;
    }

    return false;
}

bool PlayerBossBars::remove(uint32_t id) {
    const std::lock_guard<std::mutex> lock(mMutex);
    const auto found = std::find_if(mBars.begin(), mBars.end(), [id](const Bar &bar) {
        return bar.mId == id;
    });
    if (found == mBars.end())
        return false;

    mBars.erase(found);
    return true;
}

void PlayerBossBars::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    Bar wanted;
    bool hasBar = false;
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (!mBars.empty()) {
            wanted = mBars.back();
            hasBar = true;
        }
    }

    if (player.getRaidState().isBossBarShown()) {
        // the raid's create for the same actor already replaced this bar, so a remove would delete the raid bar
        mShown = false;
        return;
    }

    if (!hasBar) {
        if (mShown)
            _send(owner, player, BossEventPacket::Action::Remove, mDisplayed);
        mShown = false;
        return;
    }

    if (!mShown) {
        _send(owner, player, BossEventPacket::Action::Create, wanted);
        mShown = true;
        mDisplayed = wanted;
        return;
    }

    if (wanted.mTitle != mDisplayed.mTitle)
        _send(owner, player, BossEventPacket::Action::UpdateName, wanted);
    if (wanted.mPercent != mDisplayed.mPercent)
        _send(owner, player, BossEventPacket::Action::UpdatePercentage, wanted);
    if (wanted.mColor != mDisplayed.mColor)
        _send(owner, player, BossEventPacket::Action::UpdateStyle, wanted);
    mDisplayed = wanted;
}

void PlayerBossBars::_send(ServerNetworkHandler &owner, ServerPlayer &player, BossEventPacket::Action action,
                           const Bar &bar) {
    BossEventPacket packet;
    packet.mBossUniqueActorId = player.getUniqueId();
    packet.mAction = action;
    packet.mTitle = bar.mTitle;
    packet.mHealthPercentage = bar.mPercent;
    packet.mColor = (int32_t) bar.mColor;
    packet.mOverlay = BAR_OVERLAY_PROGRESS;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}
