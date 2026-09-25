#include "Plugin/PluginServerApi.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    bool isValidGameMode(FalconGameMode gameMode) {
        return gameMode == FALCON_GAME_MODE_SURVIVAL || gameMode == FALCON_GAME_MODE_CREATIVE ||
               gameMode == FALCON_GAME_MODE_ADVENTURE || gameMode == FALCON_GAME_MODE_SPECTATOR;
    }

    FalconGameMode playerGameMode(FalconPlayer *target) {
        return (FalconGameMode) player(target)->getGameType();
    }

    void playerSetGameMode(FalconPlayer *target, FalconGameMode gameMode) {
        if (!isValidGameMode(gameMode))
            return;
        owner().setPlayerGameMode(*player(target), (int) gameMode);
    }

    const char *playerXuid(FalconPlayer *target) {
        return hold(player(target)->getXuid());
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
            owner().getOpList().addOp(value->getName());
        else
            owner().getOpList().removeOp(value->getName());
        owner().setPlayerOp(*value, isOp);
    }
}

void PluginServerApi::fillPlayers(FalconServerApi &api) {
    api.playerGameMode = &playerGameMode;
    api.playerSetGameMode = &playerSetGameMode;
    api.playerXuid = &playerXuid;
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
