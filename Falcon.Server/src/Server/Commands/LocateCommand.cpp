#include "Server/Commands/LocateCommand.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeSearch.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/PlayerCommandOrigin.h"

#include <atomic>
#include <cmath>
#include <string>
#include <thread>

namespace {
    // an empty search runs once more from here
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
                  "/locate biome <biomeName> [teleport] | /locate structure <structure>"),
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

    CommandParamData structureModeParameter;
    structureModeParameter.mName = "mode";
    structureModeParameter.mHasEnumData = true;
    structureModeParameter.mEnumData.mName = "LocateModeStructure";
    structureModeParameter.mEnumData.mIsSoft = false;
    structureModeParameter.mEnumData.mValues = {"structure"};

    CommandParamData structureParameter;
    structureParameter.mName = "structure";
    structureParameter.mHasEnumData = true;
    structureParameter.mEnumData.mName = "Structure";
    structureParameter.mEnumData.mIsSoft = false;
    for (const StructureFeature *feature: StructureFeature::getAll())
        structureParameter.mEnumData.mValues.push_back(feature->getName());

    CommandOverloadData structureOverload;
    structureOverload.mParameters.push_back(structureModeParameter);
    structureOverload.mParameters.push_back(structureParameter);

    return {overload, structureOverload};
}

bool LocateCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (arguments[0] == "structure")
        return _locateStructure(sender, arguments);

    if (arguments[0] != "biome") {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    return _locateBiome(sender, arguments);
}

bool LocateCommand::_locateStructure(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (!sender.isPlayer()) {
        sender.sendTranslation("commands.locate.structure.fail.noplayer", {});
        return false;
    }

    const StructureFeature *feature = StructureFeature::get(arguments[1]);
    if (feature == nullptr) {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[1]});
        return false;
    }

    bool expected = false;
    if (!gSearchRunning.compare_exchange_strong(expected, true)) {
        sender.sendMessage("A search is already running");
        return false;
    }

    ServerPlayer *player = sender.asPlayer();
    const Vector3f &origin = player->getPosition();
    const int32_t originX = (int32_t) std::floor(origin.x);
    const int32_t originZ = (int32_t) std::floor(origin.z);

    const NetworkIdentifier requesterId = player->getNetworkIdentifier();
    CommandOriginData requestOrigin;
    if (const PlayerCommandOrigin *playerOrigin = dynamic_cast<const PlayerCommandOrigin *>(&sender))
        requestOrigin = playerOrigin->getOriginData();
    ServerNetworkHandler &handler = mHandler;
    Level &level = handler.getLevelFor(*player);
    const std::shared_ptr<const ChunkGenerator> generator = level.shareGenerator();

    std::thread search([&handler, generator, feature, requesterId, requestOrigin, originX, originZ]() {
        StructureChunkPos start;
        const bool found = generator->getDimensionType() == DimensionType::Overworld
                           && feature->findNearest(*generator, originX, originZ, start);
        gSearchRunning = false;

        handler.postToMainThread([&handler, feature, requesterId, requestOrigin, found, start, originX, originZ]() {
            auto entry = handler.getPlayers().find(requesterId);
            if (entry == handler.getPlayers().end())
                return;

            ServerPlayer *target = &entry->second;
            if (!target->isSpawned())
                return;

            if (!found) {
                target->sendTranslation("commands.locate.structure.fail.nostructurefound", {});
                return;
            }

            const int32_t foundX = start.x * 16 + feature->getBlockOffset();
            const int32_t foundZ = start.z * 16 + feature->getBlockOffset();
            const int64_t deltaX = foundX - originX;
            const int64_t deltaZ = foundZ - originZ;
            const int64_t distance = (int64_t) std::sqrt((double) (deltaX * deltaX + deltaZ * deltaZ));

            handler.sendCommandOutput(*target, requestOrigin, "commands.locate.structure.success",
                                      {"minecraft:" + feature->getName(), std::to_string(foundX),
                                       std::to_string(foundZ), std::to_string(distance)});
        });
    });

    search.detach();
    return true;
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

    // shared: a world unloaded mid-search doesn't take the generator with it
    const std::shared_ptr<const ChunkGenerator> generator = level.shareGenerator();

    std::thread search([&handler, generator, requesterId, requestOrigin, biomeName, biomeId, teleport, originX, originY,
                        originZ]() {
        BiomeSearch::Result result = BiomeSearch::findNearest(*generator, Vector3i(originX, originY, originZ), biomeId);
        if (!result.mFound) {
            result = BiomeSearch::findNearest(
                    *generator, Vector3i(originX + RETRY_OFFSET, originY + RETRY_OFFSET, originZ + RETRY_OFFSET),
                    biomeId);
        }
        gSearchRunning = false;

        handler.postToMainThread([&handler, generator, requesterId, requestOrigin, biomeName, biomeId, teleport, result,
                                  originX, originZ]() {
            auto entry = handler.getPlayers().find(requesterId);
            if (entry == handler.getPlayers().end())
                return;

            ServerPlayer *target = &entry->second;
            if (!target->isSpawned())
                return;

            // the searched world may have been unloaded meanwhile: resolve the level again here, and the held
            // generator tells whether it is still the one searched
            Level &level = handler.getLevelFor(*target);
            if (!target->isIn(level) || level.shareGenerator() != generator)
                return;

            if (!result.mFound) {
                target->sendTranslation("commands.locate.biome.fail", {biomeName});
                return;
            }

            const Vector3i found = result.mNeedsOpening ? BiomeSearch::findOpening(level, result.mPosition, biomeId)
                                                        : result.mPosition;

            const int32_t deltaX = found.x - originX;
            const int32_t deltaZ = found.z - originZ;
            const int32_t distance = (int32_t) std::sqrt((double) (deltaX * deltaX + deltaZ * deltaZ));

            // the client disconnects on a TextPacket with more than four parameters: the five-parameter result
            // goes out as command output, as the game sends it
            handler.sendCommandOutput(*target, requestOrigin, "commands.locate.biome.success",
                                      {biomeName, std::to_string(found.x), std::to_string(found.y),
                                       std::to_string(found.z), std::to_string(distance)});

            if (!teleport)
                return;

            // the report only reads loaded chunks, but landing in unloaded rock suffocates:
            // load the area, find the opening again, land on a standable block
            Vector3i destination = found;
            if (result.mNeedsOpening) {
                BiomeSearch::loadOpeningArea(level, result.mPosition);
                destination = BiomeSearch::findOpening(level, result.mPosition, biomeId);
            }
            destination = level.findSafeSpawn(destination);
            target->teleport(handler, Vector3f((float) destination.x, (float) destination.y, (float) destination.z));
        });
    });

    search.detach();
    return true;
}
