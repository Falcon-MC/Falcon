#include "Network/ServerNetworkHandler.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BedBlock.h"
#include "World/Level/Level.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/AnimatePacket.h"

#include <cmath>

namespace {
    const int64_t DAY_LENGTH_TICKS = 24000;

    bool countsForSleep(const ServerPlayer &player, const Level &level) {
        return player.isSpawned() && player.isIn(level) && player.getGameType() != (int32_t) GameType::Spectator;
    }
}

int32_t ServerNetworkHandler::getRequiredSleepers(int32_t players, int32_t percentage) {
    const float required = (float) players * ((float) percentage / 100.0f);
    const int32_t rounded = (int32_t) std::ceil(required);
    return rounded >= 2 ? rounded : 1;
}

bool ServerNetworkHandler::sleepOn(ServerPlayer &player, const Vector3i &head) {
    for (const auto &entry: mPlayers) {
        const ServerPlayer &other = entry.second;
        if (&other != &player && other.isSleeping() && other.getSleepingPosition() == head)
            return false;
    }

    player.setSleeping(head);
    player.resetSleepCounter();
    player.setSpawnPoint(head, player.getSpawnStandPositionHere(), player.getDimension(),
                         getWorldFor(player).getName());
    player.getFlags().set(ActorFlag::Sleeping, true);
    player.teleport(*this, Vector3f((float) head.x + 0.5f, (float) head.y + 0.5f, (float) head.z + 0.5f));
    BedBlock::setOccupied(*this, getLevelFor(player), head, true);
    _sendEntityData(player);

    _updateSleepingPlayerList(getWorldFor(player));
    return true;
}

void ServerNetworkHandler::stopSleep(ServerPlayer &player) {
    if (!player.isSleeping())
        return;

    const Vector3i head = player.getSleepingPosition();
    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_BED_LEAVE)) {
        PluginEvent bedEvent;
        bedEvent.mType = FALCON_EVENT_PLAYER_BED_LEAVE;
        bedEvent.mPlayer = &player;
        bedEvent.mBlockPosition = head;
        plugins->dispatch(bedEvent);
    }
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

    _updateSleepingPlayerList(getWorldFor(player));

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

void ServerNetworkHandler::_updateSleepingPlayerList(World &world) {
    const Level &level = world.getOverworld();
    int32_t players = 0;
    int32_t sleeping = 0;
    for (const auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (!countsForSleep(player, level))
            continue;

        players++;
        if (player.isSleeping())
            sleeping++;
    }

    const int32_t percentage = level.getGameRules().getInt("playerssleepingpercentage");
    world.mEnoughPlayersSleeping = players > 0 && sleeping >= getRequiredSleepers(players, percentage);
}

void ServerNetworkHandler::_tickSleep() {
    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping())
            entry.second.tickSleepCounter();
    }

    for (World *world: mWorlds.getWorlds())
        _tickSleep(*world);
}

void ServerNetworkHandler::_tickSleep(World &world) {
    if (!world.mEnoughPlayersSleeping)
        return;

    Level &level = world.getOverworld();
    int32_t players = 0;
    int32_t deepSleeping = 0;
    for (const auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (!countsForSleep(player, level))
            continue;

        players++;
        if (player.isSleeping() && player.isSleepingLongEnough())
            deepSleeping++;
    }

    const int32_t percentage = level.getGameRules().getInt("playerssleepingpercentage");
    if (players == 0 || deepSleeping < getRequiredSleepers(players, percentage))
        return;

    world.mEnoughPlayersSleeping = false;
    if (!level.isNight() && !level.isThundering())
        return;

    level.setTime(level.getTime() + DAY_LENGTH_TICKS - level.getDayTime());
    broadcastWorldTime(world);

    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping() && entry.second.isIn(level))
            stopSleep(entry.second);
    }
}
