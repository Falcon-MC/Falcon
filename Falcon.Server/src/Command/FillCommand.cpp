#include "Command/FillCommand.h"

#include "Actor/ServerPlayer.h"
#include "Command/SetBlockCommand.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    const std::vector<std::string> MODES = {"replace", "destroy", "keep", "hollow", "outline"};

    bool isShell(const Vector3i &position, const Vector3i &minimum, const Vector3i &maximum) {
        return position.x == minimum.x || position.x == maximum.x
               || position.y == minimum.y || position.y == maximum.y
               || position.z == minimum.z || position.z == maximum.z;
    }
}

FillCommand::FillCommand(ServerNetworkHandler &handler)
        : Command("fill", "commands.fill.description",
                  "/fill <x1> <y1> <z1> <x2> <y2> <z2> <block> [replace|destroy|keep|hollow|outline]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> FillCommand::getOverloads() const {
    CommandParamData from;
    from.mName = "from";
    from.mHasType = true;
    from.mType = CommandParamType::BlockPosition;

    CommandParamData to;
    to.mName = "to";
    to.mHasType = true;
    to.mType = CommandParamType::BlockPosition;

    CommandParamData block;
    block.mName = "block";
    block.mHasType = true;
    block.mType = CommandParamType::String;

    CommandParamData mode;
    mode.mName = "mode";
    mode.mOptional = true;
    mode.mHasEnumData = true;
    mode.mEnumData.mName = "FillMode";
    mode.mEnumData.mValues = MODES;

    CommandOverloadData overload;
    overload.mParameters.push_back(from);
    overload.mParameters.push_back(to);
    overload.mParameters.push_back(block);
    overload.mParameters.push_back(mode);

    return {overload};
}

bool FillCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 7 || arguments.size() > 8) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    ServerPlayer *self = sender.asPlayer();
    if (self == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = self->getPosition();
    const Vector3i originBlock((int32_t) std::floor(origin.x), (int32_t) std::floor(origin.y),
                               (int32_t) std::floor(origin.z));

    Vector3i from;
    Vector3i to;
    if (!parseBlockPosition(arguments, 0, originBlock, from) || !parseBlockPosition(arguments, 3, originBlock, to)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    BlockState state;
    if (!SetBlockCommand::resolveBlock(arguments[6], state)) {
        sender.sendTranslation("commands.setblock.notFound", {arguments[6]});
        return false;
    }

    const std::string mode = arguments.size() == 8 ? arguments[7] : "replace";
    if (std::find(MODES.begin(), MODES.end(), mode) == MODES.end()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Level &level = mHandler.getLevelFor(*self);

    const Vector3i minimum(std::min(from.x, to.x), std::min(from.y, to.y), std::min(from.z, to.z));
    const Vector3i maximum(std::max(from.x, to.x), std::max(from.y, to.y), std::max(from.z, to.z));

    if (minimum.y < level.getMinY() || maximum.y > level.getMaxY()) {
        sender.sendTranslation("commands.fill.outOfWorld", {});
        return false;
    }

    const int64_t width = (int64_t) maximum.x - minimum.x + 1;
    const int64_t height = (int64_t) maximum.y - minimum.y + 1;
    const int64_t depth = (int64_t) maximum.z - minimum.z + 1;
    const int64_t volume = width * height * depth;

    if (volume > MAX_BLOCKS) {
        sender.sendTranslation("commands.fill.tooManyBlocks",
                               {std::to_string(volume), std::to_string(MAX_BLOCKS)});
        return false;
    }

    const BlockState air("minecraft:air");
    const std::string placementMode = mode == "destroy" ? "destroy" : (mode == "keep" ? "keep" : "replace");

    int32_t placed = 0;
    for (int32_t x = minimum.x; x <= maximum.x; ++x) {
        for (int32_t y = minimum.y; y <= maximum.y; ++y) {
            for (int32_t z = minimum.z; z <= maximum.z; ++z) {
                const Vector3i position(x, y, z);
                const bool shell = isShell(position, minimum, maximum);

                if (mode == "outline" && !shell)
                    continue;

                const BlockState &target = mode == "hollow" && !shell ? air : state;

                if (SetBlockCommand::placeBlock(mHandler, level, position, target, placementMode))
                    ++placed;
            }
        }
    }

    if (placed == 0) {
        sender.sendTranslation("commands.fill.failed", {});
        return false;
    }

    sender.sendTranslation("commands.fill.success", {std::to_string(placed)});
    return true;
}
