#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ActorFlags.h"
#include "Actor/ServerPlayer.h"
#include "Block/Blocks/BedBlock.h"
#include "Level/Level.h"
#include "Protocol/Packets/AnimatePacket.h"

namespace {
    const int32_t SLEEP_CHECK_DELAY_TICKS = 75;
    const int64_t DAY_LENGTH_TICKS = 24000;
}

bool ServerNetworkHandler::sleepOn(ServerPlayer &player, const Vector3i &head) {
    for (const auto &entry: mPlayers) {
        const ServerPlayer &other = entry.second;
        if (&other != &player && other.isSleeping() && other.getSleepingPosition() == head)
            return false;
    }

    player.setSleeping(head);
    player.setSpawnPoint(head);
    player.getFlags().set(ActorFlag::Sleeping, true);
    player.teleport(*this, Vector3f((float) head.x + 0.5f, (float) head.y + 0.5f, (float) head.z + 0.5f));
    BedBlock::setOccupied(*this, getLevelFor(player), head, true);
    _sendEntityData(player);

    mSleepTicks = SLEEP_CHECK_DELAY_TICKS;
    return true;
}

void ServerNetworkHandler::stopSleep(ServerPlayer &player) {
    if (!player.isSleeping())
        return;

    const Vector3i head = player.getSleepingPosition();
    player.clearSleeping();
    player.getFlags().set(ActorFlag::Sleeping, false);
    _sendEntityData(player);

    bool bedStillUsed = false;
    for (const auto &entry: mPlayers) {
        if (entry.second.isSleeping() && entry.second.getSleepingPosition() == head)
            bedStillUsed = true;
    }
    if (!bedStillUsed)
        BedBlock::setOccupied(*this, getLevelFor(player), head, false);

    mSleepTicks = 0;

    AnimatePacket wakeUp;
    wakeUp.mRuntimeActorId = player.getRuntimeId();
    wakeUp.mAction = AnimatePacket::Action::WakeUp;
    mNetworkHandler->send(player.getNetworkIdentifier(), wakeUp, mCodecContext);
}

void ServerNetworkHandler::wakeSleepersAt(const Vector3i &head) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping() && entry.second.getSleepingPosition() == head)
            stopSleep(entry.second);
    }
}

void ServerNetworkHandler::_tickSleep() {
    if (mSleepTicks <= 0 || --mSleepTicks > 0)
        return;

    int players = 0;
    int sleeping = 0;
    for (const auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getDimension() != DimensionType::Overworld)
            continue;

        players++;
        if (player.isSleeping())
            sleeping++;
    }

    const int32_t percentage = mLevel.getGameRules().getInt("playerssleepingpercentage");
    if (players == 0 || sleeping == 0 || sleeping * 100 / players < percentage)
        return;

    if (!mLevel.isNight() && !mLevel.isThundering())
        return;

    mLevel.setTime(mLevel.getTime() + DAY_LENGTH_TICKS - mLevel.getDayTime());
    broadcastWorldTime();

    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping())
            stopSleep(entry.second);
    }
}
