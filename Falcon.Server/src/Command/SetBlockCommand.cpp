#include "Command/SetBlockCommand.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockPaletteRegistry.h"
#include "Block/BlockSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    const std::vector<std::string> MODES = {"replace", "destroy", "keep"};
}

SetBlockCommand::SetBlockCommand(ServerNetworkHandler &handler)
        : Command("setblock", "commands.setblock.description", "/setblock <x> <y> <z> <block> [replace|destroy|keep]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> SetBlockCommand::getOverloads() const {
    CommandParamData position;
    position.mName = "position";
    position.mHasType = true;
    position.mType = CommandParamType::BlockPosition;

    CommandParamData block;
    block.mName = "block";
    block.mHasType = true;
    block.mType = CommandParamType::String;

    CommandParamData mode;
    mode.mName = "mode";
    mode.mOptional = true;
    mode.mHasEnumData = true;
    mode.mEnumData.mName = "SetBlockMode";
    mode.mEnumData.mValues = MODES;

    CommandOverloadData overload;
    overload.mParameters.push_back(position);
    overload.mParameters.push_back(block);
    overload.mParameters.push_back(mode);

    return {overload};
}

bool SetBlockCommand::resolveBlock(const std::string &name, BlockState &out) {
    std::string identifier = name;
    if (identifier.find(':') == std::string::npos)
        identifier = "minecraft:" + identifier;

    BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
    registry.initialize();

    const Tag *states = registry.getDefaultStates(identifier);
    if (states == nullptr)
        return false;

    out = BlockState(identifier, *states);
    return true;
}

bool SetBlockCommand::placeBlock(ServerNetworkHandler &handler, Level &level, const Vector3i &position,
                                 const BlockState &state, const std::string &mode) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return false;

    if (!level.isChunkResident(position.x >> 4, position.z >> 4))
        return false;

    const BlockState existing = level.getBlockState(position.x, position.y, position.z);

    if (mode == "keep" && !BlockSupport::isReplaceable(existing))
        return false;

    if (mode == "destroy" && existing.mName != "minecraft:air") {
        BlockActionHandler::destroyBlock(handler, level, position, existing, true, ItemStack::air());
    } else {
        level.onBlockBroken(position, existing);
    }

    level.setBlock(position, state, false);
    level.onBlockPlaced(position, state);
    level.updateAround(position);
    return true;
}

bool SetBlockCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 4 || arguments.size() > 5) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = sender.getPosition();
    const Vector3i originBlock((int32_t) std::floor(origin.x), (int32_t) std::floor(origin.y),
                               (int32_t) std::floor(origin.z));

    Vector3i position;
    if (!parseBlockPosition(arguments, 0, originBlock, position)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    BlockState state;
    if (!resolveBlock(arguments[3], state)) {
        sender.sendTranslation("commands.setblock.notFound", {arguments[3]});
        return false;
    }

    const std::string mode = arguments.size() == 5 ? arguments[4] : "replace";
    if (std::find(MODES.begin(), MODES.end(), mode) == MODES.end()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (position.y < level->getMinY() || position.y > level->getMaxY()) {
        sender.sendTranslation("commands.setblock.outOfWorld", {});
        return false;
    }

    if (!placeBlock(mHandler, *level, position, state, mode)) {
        sender.sendTranslation("commands.setblock.noChange", {});
        return false;
    }

    sender.sendTranslation("commands.setblock.success", {});
    return true;
}
