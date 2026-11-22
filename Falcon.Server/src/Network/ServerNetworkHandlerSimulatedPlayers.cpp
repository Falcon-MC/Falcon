#include "Network/ServerNetworkHandler.h"

#include "Core/Event/GameEvents.h"
#include "Network/Handler/LoginHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <array>
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

namespace {
    constexpr int32_t SKIN_SIDE = 64;

    using Colour = std::array<uint8_t, 3>;

    // fills a rectangle of the classic skin layout
    void paint(std::string &pixels, int x, int y, int width, int height, const Colour &colour) {
        for (int row = y; row < y + height; row++) {
            for (int column = x; column < x + width; column++) {
                const size_t at = ((size_t) row * SKIN_SIDE + column) * 4;
                pixels[at] = (char) colour[0];
                pixels[at + 1] = (char) colour[1];
                pixels[at + 2] = (char) colour[2];
                pixels[at + 3] = (char) 255;
            }
        }
    }

    // a classic 64 by 64 skin with valid geometry: a client drops the connection over a player list entry whose
    // skin has empty JSON. Colours come from the name, so bots do not all look alike and keep their look
    SerializedSkin simulatedSkin(const std::string &name) {
        static const Colour SKIN_TONES[] = {{246, 207, 172}, {224, 172, 129}, {184, 131, 92}, {141, 92, 61}, {96, 63, 44}};
        static const Colour HAIR[] = {{48, 32, 22}, {92, 60, 34}, {214, 180, 102}, {30, 30, 30}, {150, 64, 40}};
        static const Colour CLOTHES[] = {{40, 120, 200}, {200, 60, 60}, {60, 160, 80}, {230, 180, 40}, {130, 70, 180},
                                         {40, 170, 170}, {230, 120, 40}, {90, 90, 100}};
        const size_t seed = std::hash<std::string>()(name);
        const Colour &tone = SKIN_TONES[seed % 5];
        const Colour &hair = HAIR[(seed / 5) % 5];
        const Colour &shirt = CLOTHES[(seed / 25) % 8];
        const Colour &pants = CLOTHES[(seed / 200 + 3) % 8];
        const Colour shoes = {50, 50, 55};

        std::string pixels((size_t) SKIN_SIDE * SKIN_SIDE * 4, '\0');
        paint(pixels, 0, 0, 32, 16, tone);            // head
        paint(pixels, 8, 0, 8, 8, hair);              // top of the head
        paint(pixels, 0, 8, 32, 2, hair);             // hair around the head
        paint(pixels, 8 + 1, 8 + 4, 2, 1, {40, 40, 60}); // eyes
        paint(pixels, 8 + 5, 8 + 4, 2, 1, {40, 40, 60});
        paint(pixels, 16, 16, 24, 16, shirt);         // body
        paint(pixels, 40, 16, 16, 16, shirt);         // right arm
        paint(pixels, 40, 26, 16, 6, tone);           // right hand
        paint(pixels, 32, 48, 16, 16, shirt);         // left arm
        paint(pixels, 32, 58, 16, 6, tone);           // left hand
        paint(pixels, 0, 16, 16, 16, pants);          // right leg
        paint(pixels, 0, 28, 16, 4, shoes);
        paint(pixels, 16, 48, 16, 16, pants);         // left leg
        paint(pixels, 16, 60, 16, 4, shoes);

        SerializedSkin skin;
        skin.mSkinId = "falcon.simulated." + std::to_string(seed % 100000);
        skin.mFullSkinId = skin.mSkinId;
        skin.mSkinResourcePatch = "{\"geometry\":{\"default\":\"geometry.humanoid.custom\"}}";
        skin.mGeometryData = "{}";
        skin.mGeometryDataEngineVersion = "0.0.0";
        skin.mSkinData.mWidth = SKIN_SIDE;
        skin.mSkinData.mHeight = SKIN_SIDE;
        skin.mSkinData.mData = std::move(pixels);
        return skin;
    }
}

// no saved data, the given place, game mode and yaw, then the usual first spawn
ServerPlayer *ServerNetworkHandler::spawnSimulatedPlayer(const std::string &name, Level &level,
                                                         const Vector3f &position, float yaw, int32_t gameType) {
    // a rehash would break any walk over mPlayers this spawn runs inside; a plain insert keeps its iterators
    if ((float) (mPlayers.size() + 1) > mPlayers.max_load_factor() * (float) mPlayers.bucket_count())
        return nullptr;
    const NetworkIdentifier id("simulated-player", ++mSimulatedPlayerCount);
    mNetworkHandler->addLocalSession(id);
    ServerPlayer &player = mPlayers.try_emplace(id, id, allocateRuntimeId(), this).first->second;
    player.makeSimulated();
    player.setName(name);
    player.setUuid(simulatedUuid(name, player.getRuntimeId()));
    player.setXuid("");
    player.setSkin(simulatedSkin(name));
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
    // windows open for it as for a client: a plugin's menu, a chest a script opens
    player.getInventoryManager().attach(&player, this);

    _addToPlayerList(player);
    LoginHandler::handleSetLocalPlayerAsInitialized(*this, player);

    PlayerJoinAfterEvent joinEvent(player);
    mEventBus.after().mPlayerJoin.emit(joinEvent);
    PlayerSpawnAfterEvent spawnEvent(player, true);
    mEventBus.after().mPlayerSpawn.emit(spawnEvent);
    return &player;
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

// no walk over mPlayers is active here; reserve is a no-op once the buckets fit
void ServerNetworkHandler::_reserveSimulatedSpawns() {
    mPlayers.reserve(mPlayers.size() + SIMULATED_SPAWN_HEADROOM);
}

// the input a client would send this tick, played by the same movement pipeline
void ServerNetworkHandler::_tickSimulatedPlayer(ServerPlayer &player) {
    SimulatedPlayerController *controller = player.getSimulatedController();
    if (controller == nullptr || !player.isSpawned())
        return;
    // a dead player's build stops
    if (player.isDead()) {
        controller->stopBuild();
        return;
    }
    PlayerAuthInputPacket packet;
    controller->buildInput(*this, player, packet);
    player.getMovementInputs().enqueue(packet);
    player.markActive(mCurrentTick);
}

// Player::respawn, as a client's ready respawn runs it
void ServerNetworkHandler::respawnSimulatedPlayer(ServerPlayer &player) {
    _respawnPlayer(player);
}
