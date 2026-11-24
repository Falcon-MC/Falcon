#include "Falcon/Plugin/PluginServerApi.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/Codec/ProtocolCodecRegistry.h"
#include "World/Level/Level.h"

#include <cctype>
#include <cmath>
#include <string>

using namespace PluginApiHelpers;

namespace {
    bool isValidGameMode(FalconGameMode gameMode) {
        return gameMode == FALCON_GAME_MODE_SURVIVAL || gameMode == FALCON_GAME_MODE_CREATIVE
               || gameMode == FALCON_GAME_MODE_ADVENTURE || gameMode == FALCON_GAME_MODE_SPECTATOR;
    }

    void playerSetVisibleTo(FalconPlayer *target, FalconPlayer *viewer, int visible) {
        if (target != nullptr && viewer != nullptr)
            owner().setPlayerVisibleTo(*player(viewer), *player(target), visible != 0);
    }

    void playerSpawnTo(FalconPlayer *target, FalconPlayer *viewer) {
        if (target != nullptr && viewer != nullptr)
            owner().spawnPlayerTo(*player(viewer), *player(target));
    }

    void playerDespawnFrom(FalconPlayer *target, FalconPlayer *viewer, int sendPacket) {
        if (target != nullptr && viewer != nullptr)
            owner().despawnPlayerFrom(*player(viewer), *player(target), sendPacket != 0);
    }

    int playerIsSpawnedTo(FalconPlayer *target, FalconPlayer *viewer) {
        return target != nullptr && viewer != nullptr
               && player(viewer)->getVisiblePlayers().count(player(target)->getRuntimeId()) != 0;
    }

    constexpr size_t MAX_SIMULATED_NAME = 32;

    bool sameName(const std::string &a, const std::string &b) {
        if (a.size() != b.size())
            return false;
        for (size_t index = 0; index < a.size(); index++) {
            if (std::tolower((unsigned char) a[index]) != std::tolower((unsigned char) b[index]))
                return false;
        }
        return true;
    }

    // a printable name no one online uses, a loaded level and a place inside its height; as many as the plugin
    // wants, for load tests
    FalconPlayer *serverSpawnSimulatedPlayer(const char *name, FalconLevel *destination, FalconVec3 position,
                                             float yaw, FalconGameMode gameMode) {
        if (name == nullptr || destination == nullptr || !isValidGameMode(gameMode) || !std::isfinite(yaw)
            || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            return nullptr;
        const std::string playerName = name;
        if (playerName.empty() || playerName.size() > MAX_SIMULATED_NAME)
            return nullptr;
        for (unsigned char c: playerName) {
            if (c < 0x20 || c == 0x7f)
                return nullptr;
        }
        Level &target = *level(destination);
        if (position.y < target.getMinY() || position.y > target.getMaxY())
            return nullptr;

        ServerNetworkHandler &host = owner();
        for (auto &entry: host.getPlayers()) {
            if (sameName(entry.second.getName(), playerName))
                return nullptr;
        }
        return toHandle(host.spawnSimulatedPlayer(playerName, target,
                                                  Vector3f((float) position.x, (float) position.y, (float) position.z),
                                                  yaw, (int32_t) gameMode));
    }

    int playerIsSimulated(FalconPlayer *target) {
        return player(target)->isSimulated() ? 1 : 0;
    }

    // leaves at the end of the tick, as a disconnect
    int playerRemoveSimulated(FalconPlayer *target) {
        ServerPlayer *value = player(target);
        if (!value->isSimulated())
            return 0;
        owner().removeSimulatedPlayer(value->getNetworkIdentifier());
        return 1;
    }

    FalconGameMode playerGameMode(FalconPlayer *target) {
        return (FalconGameMode) player(target)->getGameType();
    }

    void playerSetGameMode(FalconPlayer *target, FalconGameMode gameMode) {
        if (!isValidGameMode(gameMode))
            return;
        owner().setPlayerGameMode(*player(target), (int) gameMode);
    }

    int playerAllowFlight(FalconPlayer *target) {
        return player(target)->canToggleFlight();
    }

    void playerSetAllowFlight(FalconPlayer *target, int allowed) {
        ServerPlayer &value = *player(target);
        value.setAllowFlight(allowed != 0);
        if (value.getLoginState() >= ServerPlayer::LoginState::StartGameSent)
            owner()._sendAbilities(value);
    }

    int playerIsFlying(FalconPlayer *target) {
        return player(target)->isFlying();
    }

    void playerSetFlying(FalconPlayer *target, int flying) {
        ServerPlayer &value = *player(target);
        value.setFlying(flying != 0);
        if (value.getLoginState() < ServerPlayer::LoginState::StartGameSent)
            return;

        ServerNetworkHandler &host = owner();
        if (!flying)
            value.setOnGround(MovementHandler::checkGroundState(host.getLevelFor(value), value.getPosition(), &value));
        host._sendAbilities(value);
    }

    const char *playerXuid(FalconPlayer *target) {
        return hold(player(target)->getXuid());
    }

    int32_t playerProtocolVersion(FalconPlayer *target) {
        const std::shared_ptr<const ProtocolCodec> &codec = player(target)->getCodec();
        return codec != nullptr ? codec->getProtocolVersion()
                                : ProtocolCodecRegistry::instance().getDefault().getProtocolVersion();
    }

    // the id packets name the player by and their saved data keeps, not the runtime id of this session
    int64_t playerUniqueId(FalconPlayer *target) {
        return player(target)->getUniqueId();
    }

    const char *playerUuid(FalconPlayer *target) {
        return hold(player(target)->getUuid());
    }

    const char *playerAddress(FalconPlayer *target) {
        return hold(player(target)->getNetworkIdentifier().getAddress());
    }

    void playerSendTitle(FalconPlayer *target, const char *title, const char *subtitle) {
        const std::string titleText = title == nullptr ? std::string() : std::string(title);
        const std::string subtitleText = subtitle == nullptr ? std::string() : std::string(subtitle);
        player(target)->sendTitle(titleText, subtitleText);
    }

    void playerSendActionBar(FalconPlayer *target, const char *message) {
        if (message != nullptr)
            player(target)->sendActionBar(message);
    }

    float playerFood(FalconPlayer *target) {
        return player(target)->getFood();
    }

    void playerSetFood(FalconPlayer *target, float food) {
        ServerPlayer *value = player(target);
        value->setFood(food);
        owner().syncPlayerAttributes(*value);
    }

    int32_t playerXpLevel(FalconPlayer *target) {
        return player(target)->getExperience().getXpLevel();
    }

    void playerSetXpLevel(FalconPlayer *target, int32_t level) {
        ServerPlayer *value = player(target);
        value->setXpAndProgress(level < 0 ? 0 : level, value->getExperience().getXpProgress());
        owner().syncPlayerAttributes(*value);
    }

    void playerSetOperator(FalconPlayer *target, int operator_) {
        ServerPlayer *value = player(target);
        const bool isOp = operator_ != 0;
        if (isOp)
            owner().getOpList().addOp(value->getName(), value->getXuid());
        else
            owner().getOpList().removeOp(value->getName());
        owner().setPlayerOp(*value, isOp);
    }
}

namespace {
    int playerHasChunk(FalconPlayer *target, int32_t chunkX, int32_t chunkZ) {
        ServerPlayer *value = player(target);
        return value->getSentChunks().count(ChunkStreamHandler::packChunk(chunkX, chunkZ)) != 0 ? 1 : 0;
    }
}

void PluginServerApi::fillPlayers(FalconServerApi &api) {
    api.playerHasChunk = &playerHasChunk;
    api.playerGameMode = &playerGameMode;
    api.playerSetGameMode = &playerSetGameMode;
    api.playerSetVisibleTo = &playerSetVisibleTo;
    api.playerSpawnTo = &playerSpawnTo;
    api.playerDespawnFrom = &playerDespawnFrom;
    api.playerIsSpawnedTo = &playerIsSpawnedTo;
    api.playerAllowFlight = &playerAllowFlight;
    api.playerSetAllowFlight = &playerSetAllowFlight;
    api.playerIsFlying = &playerIsFlying;
    api.playerSetFlying = &playerSetFlying;
    api.serverSpawnSimulatedPlayer = &serverSpawnSimulatedPlayer;
    api.playerIsSimulated = &playerIsSimulated;
    api.playerRemoveSimulated = &playerRemoveSimulated;
    api.playerXuid = &playerXuid;
    api.playerProtocolVersion = &playerProtocolVersion;
    api.playerUniqueId = &playerUniqueId;
    api.playerUuid = &playerUuid;
    api.playerAddress = &playerAddress;
    api.playerSendTitle = &playerSendTitle;
    api.playerSendActionBar = &playerSendActionBar;
    api.playerFood = &playerFood;
    api.playerSetFood = &playerSetFood;
    api.playerXpLevel = &playerXpLevel;
    api.playerSetXpLevel = &playerSetXpLevel;
    api.playerSetOperator = &playerSetOperator;
}
