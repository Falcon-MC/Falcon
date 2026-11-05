#include "Server/Commands/LocateCommand.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeSearch.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/PlayerCommandOrigin.h"

#include <atomic>
#include <cmath>
#include <string>
#include <thread>

namespace {
    // a search that finds nothing is run once more from here (sub_14BEA2690, xmmword_14F1FBCD0)
    const int32_t RETRY_OFFSET = 32;

    std::atomic<bool> gSearchRunning{false};

    bool parseBoolean(const std::string &value, bool &out) {
        if (value == "true") {
            out = true;
            return true;
        }

        if (value == "false") {
            out = false;
            return true;
        }

        return false;
    }
}

LocateCommand::LocateCommand(ServerNetworkHandler &handler)
        : Command("locate", "commands.locate.description",
                  "/locate biome <biomeName> [teleport]"),
          mHandler(handler) {}

CommandPermission LocateCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

std::vector<CommandOverloadData> LocateCommand::getOverloads() const {
    CommandParamData modeParameter;
    modeParameter.mName = "mode";
    modeParameter.mHasEnumData = true;
    modeParameter.mEnumData.mName = "LocateModeBiome";
    modeParameter.mEnumData.mIsSoft = false;
    modeParameter.mEnumData.mValues = {"biome"};

    CommandParamData biomeParameter;
    biomeParameter.mName = "biomeName";
    biomeParameter.mHasEnumData = true;
    biomeParameter.mEnumData.mName = "Biome";
    biomeParameter.mEnumData.mIsSoft = false;
    biomeParameter.mEnumData.mValues = BiomeChunkGenDataRegistry::getBiomeNames();

    CommandParamData teleportParameter;
    teleportParameter.mName = "teleport";
    teleportParameter.mOptional = true;
    teleportParameter.mHasEnumData = true;
    teleportParameter.mEnumData.mName = "Boolean";
    teleportParameter.mEnumData.mIsSoft = false;
    teleportParameter.mEnumData.mValues = {"true", "false"};

    CommandOverloadData overload;
    overload.mParameters.push_back(modeParameter);
    overload.mParameters.push_back(biomeParameter);
    overload.mParameters.push_back(teleportParameter);

    return {overload};
}

bool LocateCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (arguments[0] != "biome") {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    return _locateBiome(sender, arguments);
}

bool LocateCommand::_locateBiome(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (!sender.isPlayer()) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const std::string &biomeName = arguments[1];
    const int32_t biomeId = BiomeChunkGenDataRegistry::getBiomeId(biomeName);
    if (biomeId < 0) {
        sender.sendTranslation("commands.generic.parameter.invalid", {biomeName});
        return false;
    }

    bool teleport = false;
    if (arguments.size() > 2 && !parseBoolean(arguments[2], teleport)) {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[2]});
        return false;
    }

    bool expected = false;
    if (!gSearchRunning.compare_exchange_strong(expected, true)) {
        sender.sendMessage("A biome search is already running");
        return false;
    }

    ServerPlayer *player = sender.asPlayer();
    const Vector3f &origin = player->getPosition();
    const int32_t originX = (int32_t) std::floor(origin.x);
    const int32_t originY = (int32_t) std::floor(origin.y);
    const int32_t originZ = (int32_t) std::floor(origin.z);

    const NetworkIdentifier requesterId = player->getNetworkIdentifier();
    CommandOriginData requestOrigin;
    if (const PlayerCommandOrigin *playerOrigin = dynamic_cast<const PlayerCommandOrigin *>(&sender))
        requestOrigin = playerOrigin->getOriginData();
    ServerNetworkHandler &handler = mHandler;
    Level &level = handler.getLevelFor(*player);

    // shared so a world unloaded during the search does not take the generator with it
    const std::shared_ptr<const ChunkGenerator> generator = level.shareGenerator();

    std::thread search([&handler, &level, generator, requesterId, requestOrigin, biomeName, biomeId, teleport,
                        originX, originY, originZ]() {
        BiomeSearch::Result result = BiomeSearch::findNearest(*generator, Vector3i(originX, originY, originZ),
                                                              biomeId);
        if (!result.mFound) {
            result = BiomeSearch::findNearest(*generator, Vector3i(originX + RETRY_OFFSET, originY + RETRY_OFFSET,
                                                                   originZ + RETRY_OFFSET), biomeId);
        }
        gSearchRunning = false;

        handler.postToMainThread([&handler, &level, requesterId, requestOrigin, biomeName, biomeId, teleport, result,
                                  originX, originZ]() {
            auto entry = handler.getPlayers().find(requesterId);
            if (entry == handler.getPlayers().end())
                return;

            ServerPlayer *target = &entry->second;
            if (!target->isSpawned() || !target->isIn(level))
                return;

            if (!result.mFound) {
                target->sendTranslation("commands.locate.biome.fail", {biomeName});
                return;
            }

            const Vector3i found = result.mNeedsOpening
                                   ? BiomeSearch::findOpening(level, result.mPosition, biomeId)
                                   : result.mPosition;

            const int32_t deltaX = found.x - originX;
            const int32_t deltaZ = found.z - originZ;
            const int32_t distance = (int32_t) std::sqrt((double) (deltaX * deltaX + deltaZ * deltaZ));

            // The client rejects a TextPacket with more than four parameters and disconnects, so the
            // five-parameter result goes out as command output, like the vanilla server sends it.
            handler.sendCommandOutput(*target, requestOrigin, "commands.locate.biome.success",
                                      {biomeName, std::to_string(found.x), std::to_string(found.y),
                                       std::to_string(found.z), std::to_string(distance)});

            if (teleport)
                target->teleport(handler, Vector3f((float) found.x, (float) found.y, (float) found.z));
        });
    });

    search.detach();
    return true;
}
