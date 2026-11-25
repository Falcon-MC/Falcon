#include "Server/Commands/CloneCommand.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandEnums.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
    const std::vector<std::string> MASK_MODES = {"replace", "masked"};
    const std::vector<std::string> CLONE_MODES = {"normal", "force", "move"};
    const char *const FILTERED = "filtered";
    // the game allows as many blocks as the dimension is high, times 2048
    const int32_t BLOCKS_PER_HEIGHT = 2048;

    struct Copied {
        Vector3i mTarget;
        BlockState mBlock;
        BlockState mExtra;
    };

    bool chunksResident(Level &level, const Vector3i &low, const Vector3i &high) {
        if (low.y < level.getMinY() || high.y > level.getMaxY())
            return false;
        for (int32_t chunkX = low.x >> 4; chunkX <= high.x >> 4; ++chunkX) {
            for (int32_t chunkZ = low.z >> 4; chunkZ <= high.z >> 4; ++chunkZ) {
                if (!level.isChunkResident(chunkX, chunkZ))
                    return false;
            }
        }
        return true;
    }

    bool isAir(const BlockState &state) {
        return state.mName == BlockNames::AIR;
    }

    CommandParamData position(const char *name) {
        return Command::makeTypedParameter(name, CommandParamType::BlockPosition);
    }
}

CloneCommand::CloneCommand(ServerNetworkHandler &handler)
        : Command("clone", "commands.clone.description",
                  "/clone <begin: x y z> <end: x y z> <destination: x y z> [maskMode: MaskMode] "
                  "[cloneMode: CloneMode]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> CloneCommand::getOverloads() const {
    CommandOverloadData plain;
    plain.mParameters = {position("begin"), position("end"), position("destination"),
                         makeEnumParameter("maskMode", "MaskMode", MASK_MODES, true),
                         makeEnumParameter("cloneMode", "CloneMode", CLONE_MODES, true)};

    CommandOverloadData byData;
    byData.mParameters = {position("begin"),
                          position("end"),
                          position("destination"),
                          makeEnumParameter("maskMode", "MaskModeFiltered", {FILTERED}),
                          makeEnumParameter("cloneMode", "CloneMode", CLONE_MODES),
                          makeEnumParameter("tileName", "Block", CommandEnums::blockValues()),
                          makeTypedParameter("tileData", CommandParamType::Int)};
    CommandOverloadData byStates = byData;
    byStates.mParameters.back() = makeTypedParameter("blockStates", CommandParamType::BlockStates);
    return {plain, byData, byStates};
}

bool CloneCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Level *level = sender.getLevel();
    if (level == nullptr || arguments.size() < 9) {
        sendUsageError(sender);
        return false;
    }

    const Vector3f origin = sender.getPosition();
    const Vector3f rotation = sender.getRotation();
    Vector3i begin;
    Vector3i end;
    Vector3i destination;
    if (!parseBlockPosition(arguments, 0, origin, rotation, begin)
        || !parseBlockPosition(arguments, 3, origin, rotation, end)
        || !parseBlockPosition(arguments, 6, origin, rotation, destination)) {
        sendUsageError(sender);
        return false;
    }

    const std::string maskMode = arguments.size() > 9 ? arguments[9] : MASK_MODES[0];
    const std::string cloneMode = arguments.size() > 10 ? arguments[10] : CLONE_MODES[0];
    const bool filtered = maskMode == FILTERED;
    const bool masked = maskMode == "masked";
    if ((!filtered && std::find(MASK_MODES.begin(), MASK_MODES.end(), maskMode) == MASK_MODES.end())
        || std::find(CLONE_MODES.begin(), CLONE_MODES.end(), cloneMode) == CLONE_MODES.end()) {
        sendUsageError(sender);
        return false;
    }
    std::optional<BlockState> filter;
    if (filtered) {
        BlockState block;
        if (arguments.size() < 12 || !CommandEnums::parseBlock(arguments[11], block)) {
            sender.sendTranslation("commands.clone.failed", {});
            return false;
        }
        filter = block;
    }

    const Vector3i low(std::min(begin.x, end.x), std::min(begin.y, end.y), std::min(begin.z, end.z));
    const Vector3i high(std::max(begin.x, end.x), std::max(begin.y, end.y), std::max(begin.z, end.z));
    const Vector3i size(high.x - low.x, high.y - low.y, high.z - low.z);
    const Vector3i targetHigh(destination.x + size.x, destination.y + size.y, destination.z + size.z);

    // copying onto itself needs force, or a mask that leaves blocks behind
    const bool overlap = low.x <= targetHigh.x && destination.x <= high.x && low.y <= targetHigh.y
                         && destination.y <= high.y && low.z <= targetHigh.z && destination.z <= high.z;
    if (overlap && cloneMode != "force" && maskMode == MASK_MODES[0]) {
        sender.sendTranslation("commands.clone.noOverlap", {});
        return false;
    }
    if (!chunksResident(*level, low, high) || !chunksResident(*level, destination, targetHigh)) {
        sender.sendTranslation("commands.generic.outOfWorld", {});
        return false;
    }

    // blocks counted: spans between the corners make a flat area free
    const int64_t volume = ((int64_t) high.x - low.x + 1) * ((int64_t) high.y - low.y + 1)
                           * ((int64_t) high.z - low.z + 1);
    const int64_t limit = (int64_t) (level->getMaxY() - level->getMinY() + 1) * BLOCKS_PER_HEIGHT;
    if (volume > limit) {
        sender.sendTranslation("commands.clone.tooManyBlocks", {std::to_string(volume), std::to_string(limit)});
        return false;
    }

    // read everything before writing anything; whole blocks go first, then what leans on them
    std::vector<Copied> whole;
    std::vector<Copied> leaning;
    std::vector<std::pair<Vector3i, Tag>> actors;
    std::vector<Vector3i> sources;
    for (int32_t z = low.z; z <= high.z; ++z) {
        for (int32_t y = low.y; y <= high.y; ++y) {
            for (int32_t x = low.x; x <= high.x; ++x) {
                const BlockState state = level->getBlockState(x, y, z);
                if (filter.has_value() && state.mName != filter->mName)
                    continue;
                if (masked && isAir(state))
                    continue;

                const Vector3i source(x, y, z);
                const Vector3i target(destination.x + x - low.x, destination.y + y - low.y,
                                      destination.z + z - low.z);
                sources.push_back(source);
                if (BlockActor *actor = level->getBlockActors().find(source))
                    actors.emplace_back(target, actor->saveWithPosition());
                const BlockState *extra = level->peekBlockPtr(x, y, z, 1);
                Copied copied{target, state, extra != nullptr ? *extra : BlockState(BlockNames::AIR)};
                (BlockShape::isFullCube(state) ? whole : leaning).push_back(std::move(copied));
            }
        }
    }
    if (sources.empty()) {
        sender.sendTranslation("commands.clone.failed", {});
        return false;
    }

    const BlockState air(BlockNames::AIR);
    // the second layer (water around a waterlogged block) is written only where one is or was
    const auto setExtra = [this](Level &where, const Vector3i &at, const BlockState &extra) {
        const BlockState *current = where.peekBlockPtr(at.x, at.y, at.z, 1);
        if (isAir(extra) && (current == nullptr || isAir(*current)))
            return;
        where.setBlockStateAtLayer(at.x, at.y, at.z, 1, extra);
        BlockActionHandler::broadcastBlockUpdate(mHandler, where, at, extra, 1);
    };
    std::vector<Vector3i> changed;
    LiquidPhysicsSystem &liquids = level->getLiquidPhysics();
    {
        const LiquidPhysicsSystem::QuietWrites quiet(liquids);
        if (cloneMode == "move") {
            for (const Vector3i &source: sources) {
                level->getBlockActors().remove(source);
                setExtra(*level, source, air);
                level->setBlock(source, air, false);
                BlockActionHandler::broadcastBlockUpdate(mHandler, *level, source, air);
                changed.push_back(source);
            }
        }
        for (const std::vector<Copied> *list: {&whole, &leaning}) {
            for (const Copied &copied: *list) {
                level->getBlockActors().remove(copied.mTarget);
                level->setBlock(copied.mTarget, copied.mBlock, false);
                setExtra(*level, copied.mTarget, copied.mExtra);
                BlockActionHandler::broadcastBlockUpdate(mHandler, *level, copied.mTarget, copied.mBlock);
                changed.push_back(copied.mTarget);
            }
        }
        for (std::pair<Vector3i, Tag> &entry: actors) {
            entry.second.putInt("x", entry.first.x);
            entry.second.putInt("y", entry.first.y);
            entry.second.putInt("z", entry.first.z);
            level->getBlockActors().addLoaded({entry.second}, mHandler.getCodecContext());
            if (BlockActor *placed = level->getBlockActors().find(entry.first))
                BlockActionHandler::broadcastBlockActorData(mHandler, *level, *placed);
        }
    }
    for (const Vector3i &position: changed) {
        liquids.updateNeighbours(position);
        level->updateAround(position);
    }

    sender.sendTranslation("commands.clone.success", {std::to_string(whole.size() + leaning.size())});
    return true;
}
