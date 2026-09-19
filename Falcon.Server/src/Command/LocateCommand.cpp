#include "Command/LocateCommand.h"

#include "Actor/ServerPlayer.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <string>
#include <thread>

namespace {
    const int32_t SEA_LEVEL = 63;
    const int32_t HEIGHT_STEP = 8;
    const int32_t TELEPORT_HEIGHT_OFFSET = 16;

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
                  "/locate biome <biomeName> [teleport] [radius] [spiral|xaxis]"),
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

    CommandParamData radiusParameter;
    radiusParameter.mName = "radius";
    radiusParameter.mOptional = true;

    CommandParamData searchParameter;
    searchParameter.mName = "search";
    searchParameter.mOptional = true;
    searchParameter.mHasEnumData = true;
    searchParameter.mEnumData.mName = "LocateSearchType";
    searchParameter.mEnumData.mIsSoft = false;
    searchParameter.mEnumData.mValues = {"spiral", "xaxis"};

    CommandOverloadData overload;
    overload.mParameters.push_back(modeParameter);
    overload.mParameters.push_back(biomeParameter);
    overload.mParameters.push_back(teleportParameter);
    overload.mParameters.push_back(radiusParameter);
    overload.mParameters.push_back(searchParameter);

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

    int32_t maxRadius = DEFAULT_BIOME_RADIUS;
    if (arguments.size() > 3) {
        try {
            maxRadius = (int32_t) std::stol(arguments[3]);
        } catch (const std::exception &) {
            sender.sendTranslation("commands.generic.num.invalid", {arguments[3]});
            return false;
        }

        if (maxRadius <= 0) {
            sender.sendTranslation("commands.generic.num.invalid", {arguments[3]});
            return false;
        }
    }

    int32_t searchType = SEARCH_SPIRAL;
    if (arguments.size() > 4) {
        if (arguments[4] == "xaxis")
            searchType = SEARCH_X_AXIS;
        else if (arguments[4] != "spiral") {
            sender.sendTranslation("commands.generic.parameter.invalid", {arguments[4]});
            return false;
        }
    }

    ServerPlayer *player = sender.asPlayer();
    const Vector3f &origin = player->getPosition();
    const int32_t originX = (int32_t) std::floor(origin.x);
    const int32_t originY = (int32_t) std::floor(origin.y);
    const int32_t originZ = (int32_t) std::floor(origin.z);

    const NetworkIdentifier requesterId = player->getNetworkIdentifier();
    ServerNetworkHandler &handler = mHandler;
    Level &level = handler.getLevelFor(*player);

    std::thread search([&handler, &level, requesterId, biomeName, biomeId, maxRadius, searchType, teleport,
                        originX, originY, originZ]() {
        Vector3i found;
        const bool located = searchType == SEARCH_X_AXIS
                             ? _findBiomeXAxis(level, originX, originZ, biomeId, maxRadius, found)
                             : _findBiomeSpiral(level, originX, originZ, biomeId, maxRadius, found);

        handler.postToMainThread([&handler, &level, requesterId, biomeName, teleport, located, found,
                                  originX, originY, originZ]() {
            auto entry = handler.getPlayers().find(requesterId);
            if (entry == handler.getPlayers().end())
                return;

            ServerPlayer *target = &entry->second;
            if (!target->isSpawned() || target->getDimension() != level.getDimensionType())
                return;

            if (!located) {
                target->sendTranslation("commands.locate.biome.fail", {biomeName});
                return;
            }

            Vector3i result = found;
            if (level.hasSkyLight())
                result.y = level.getHeightAt(originX, originZ) + TELEPORT_HEIGHT_OFFSET;

            const double deltaX = (double) result.x - (double) originX;
            const double deltaY = (double) result.y - (double) originY;
            const double deltaZ = (double) result.z - (double) originZ;
            const int32_t distance = (int32_t) std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

            target->sendTranslation("commands.locate.biome.success",
                                    {biomeName, std::to_string(result.x), std::to_string(result.y),
                                     std::to_string(result.z), std::to_string(distance)});

            if (teleport)
                target->teleport(handler, Vector3f((float) result.x, (float) result.y, (float) result.z));
        });
    });

    search.detach();
    return true;
}

bool LocateCommand::_findBiomeSpiral(Level &level, int32_t centerX, int32_t centerZ, int32_t biomeId,
                                     int32_t maxRadius, Vector3i &found) {
    int32_t x = 0;
    int32_t z = 0;
    int32_t dx = 0;
    int32_t dz = -1;

    const int64_t diameter = (int64_t) maxRadius * 2 + 1;
    const int64_t maxSteps = diameter * diameter;

    for (int64_t step = 0; step < maxSteps; step++) {
        const int32_t checkX = centerX + (x << 4);
        const int32_t checkZ = centerZ + (z << 4);

        OverworldBiomeResult result = level.pickBiomeResult(checkX, SEA_LEVEL, checkZ);

        int32_t height = SEA_LEVEL;
        while (height > level.getMinY()) {
            if (result.correct(height - SEA_LEVEL).getBiomeId() == biomeId) {
                found = Vector3i(checkX, height, checkZ);
                return true;
            }

            height -= HEIGHT_STEP;
        }

        if (x == z || (x < 0 && x == -z) || (x > 0 && x == 1 - z)) {
            const int32_t temp = dx;
            dx = -dz;
            dz = temp;
        }

        x += dx;
        z += dz;
    }

    return false;
}

bool LocateCommand::_findBiomeXAxis(Level &level, int32_t centerX, int32_t centerZ, int32_t biomeId,
                                    int32_t maxRadius, Vector3i &found) {
    for (int32_t x = 0; x <= maxRadius; x++) {
        const int32_t offset = x << 4;

        for (int32_t i = 0; i < 2; i++) {
            const int32_t deltaX = i == 0 ? offset : -offset;
            const int32_t checkX = centerX + deltaX;

            if (level.pickBiome(checkX, SEA_LEVEL, centerZ) == biomeId) {
                found = Vector3i(checkX, SEA_LEVEL, centerZ);
                return true;
            }
        }
    }

    return false;
}
