#include "Network/ServerNetworkHandler.h"

#include "Core/Event/GameEvents.h"
#include "Network/Handler/LoginHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cstdio>
#include <functional>

namespace {
    // a list entry of its own per simulated player, whatever names they share
    std::string simulatedUuid(const std::string &name, uint64_t runtimeId) {
        const uint64_t most = std::hash<std::string>()(name) | 1;
        char text[37];
        std::snprintf(text, sizeof(text), "%08x-%04x-%04x-%04x-%012llx", (unsigned) (most >> 32),
                      (unsigned) ((most >> 16) & 0xFFFF), (unsigned) (most & 0xFFFF),
                      (unsigned) ((runtimeId >> 48) & 0xFFFF), (unsigned long long) (runtimeId & 0xFFFFFFFFFFFFULL));
        return text;
    }
}

// sub_141034460: no saved data, the given place, game mode and yaw, then the usual first spawn
ServerPlayer &ServerNetworkHandler::spawnSimulatedPlayer(const std::string &name, Level &level,
                                                         const Vector3f &position, float yaw, int32_t gameType) {
    const NetworkIdentifier id("simulated-player", ++mSimulatedPlayerCount);
    mNetworkHandler->addLocalSession(id);
    ServerPlayer &player = mPlayers.try_emplace(id, id, allocateRuntimeId(), this).first->second;
    player.makeSimulated();
    player.setName(name);
    player.setUuid(simulatedUuid(name, player.getRuntimeId()));
    player.setXuid("");
    player.getFlags().applyPlayerDefaults();
    player.getAttributes() = ActorAttributes::createPlayerDefaults();
    player.setUniqueId(allocateActorUniqueId());
    player.moveToLevel(level);
    player.setPosition(position);
    player.setRotation(Vector3f(0.0f, yaw, yaw));
    player.setGameType(gameType);
    player.setHungerEnabled(player.isSurvival() || player.isAdventure());
    player.setLoggedInTick(mCurrentTick);
    player.setSessionInspected();
    player.setSpawnChunksReady(true);
    player.setLoginState(ServerPlayer::LoginState::StartGameSent);

    _addToPlayerList(player);
    LoginHandler::handleSetLocalPlayerAsInitialized(*this, player);

    PlayerJoinAfterEvent joinEvent(player);
    mEventBus.after().mPlayerJoin.emit(joinEvent);
    PlayerSpawnAfterEvent spawnEvent(player, true);
    mEventBus.after().mPlayerSpawn.emit(spawnEvent);
    return player;
}

void ServerNetworkHandler::removeSimulatedPlayer(const NetworkIdentifier &id) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSimulated())
        return;
    for (const NetworkIdentifier &queued: mSimulatedRemovals) {
        if (queued == id)
            return;
    }
    mSimulatedRemovals.push_back(id);
}

void ServerNetworkHandler::_removeSimulatedPlayers() {
    std::vector<NetworkIdentifier> removals;
    removals.swap(mSimulatedRemovals);
    for (const NetworkIdentifier &id: removals) {
        onConnectionClosed(id, DisconnectFailReason::Disconnected, "");
        mNetworkHandler->removeLocalSession(id);
    }
}

// the input a client would send this tick, played by the same movement pipeline
void ServerNetworkHandler::_tickSimulatedPlayer(ServerPlayer &player) {
    SimulatedPlayerController *controller = player.getSimulatedController();
    if (controller == nullptr || !player.isSpawned())
        return;
    // sub_141031D90: a dead player's build stops
    if (player.isDead()) {
        controller->stopBuild();
        return;
    }
    PlayerAuthInputPacket packet;
    controller->buildInput(*this, player, packet);
    player.getMovementInputs().enqueue(packet);
    player.markActive(mCurrentTick);
}

// sub_14DCFDFA0: Player::respawn, as a client's ready respawn runs it
void ServerNetworkHandler::respawnSimulatedPlayer(ServerPlayer &player) {
    _respawnPlayer(player);
}
