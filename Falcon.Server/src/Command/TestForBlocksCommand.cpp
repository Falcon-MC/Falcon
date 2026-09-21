#include "Command/TestForBlocksCommand.h"

#include "Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const int64_t MAX_COMPARED_BLOCKS = 524288;
}

TestForBlocksCommand::TestForBlocksCommand()
        : Command("testforblocks", "commands.testforblocks.description",
                  "/testforblocks <begin> <end> <destination> [masked|all]") {}

std::vector<CommandOverloadData> TestForBlocksCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makeTypedParameter("begin", CommandParamType::BlockPosition),
                            makeTypedParameter("end", CommandParamType::BlockPosition),
                            makeTypedParameter("destination", CommandParamType::BlockPosition),
                            makeEnumParameter("mode", "TestForBlocksMode", {"masked", "all"}, true)};
    return {overload};
}

bool TestForBlocksCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = sender.getPosition();
    const Vector3i originBlock((int32_t) std::floor(origin.x), (int32_t) std::floor(origin.y),
                               (int32_t) std::floor(origin.z));

    Vector3i begin;
    Vector3i end;
    Vector3i destination;
    if (arguments.size() < 9 || !parseBlockPosition(arguments, 0, originBlock, begin)
        || !parseBlockPosition(arguments, 3, originBlock, end)
        || !parseBlockPosition(arguments, 6, originBlock, destination)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const bool masked = arguments.size() > 9 && arguments[9] == "masked";

    const Vector3i minimum(std::min(begin.x, end.x), std::min(begin.y, end.y), std::min(begin.z, end.z));
    const Vector3i maximum(std::max(begin.x, end.x), std::max(begin.y, end.y), std::max(begin.z, end.z));
    const int64_t volume = (int64_t) (maximum.x - minimum.x + 1) * (maximum.y - minimum.y + 1)
                           * (maximum.z - minimum.z + 1);

    if (volume > MAX_COMPARED_BLOCKS) {
        sender.sendTranslation("commands.compare.tooManyBlocks",
                               {std::to_string(volume), std::to_string(MAX_COMPARED_BLOCKS)});
        return false;
    }

    int64_t compared = 0;
    for (int32_t x = minimum.x; x <= maximum.x; ++x) {
        for (int32_t y = minimum.y; y <= maximum.y; ++y) {
            for (int32_t z = minimum.z; z <= maximum.z; ++z) {
                const BlockState source = level->getBlockState(x, y, z);
                if (masked && source.mName == "minecraft:air")
                    continue;

                const BlockState target = level->getBlockState(destination.x + (x - minimum.x),
                                                               destination.y + (y - minimum.y),
                                                               destination.z + (z - minimum.z));
                if (source.mName != target.mName || !(source.mStates == target.mStates)) {
                    sender.sendTranslation("commands.compare.failed", {});
                    return false;
                }

                ++compared;
            }
        }
    }

    sender.sendTranslation("commands.compare.success", {std::to_string(compared)});
    return true;
}
