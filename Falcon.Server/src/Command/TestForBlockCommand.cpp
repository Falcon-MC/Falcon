#include "Command/TestForBlockCommand.h"

#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <cmath>

TestForBlockCommand::TestForBlockCommand()
        : Command("testforblock", "commands.testforblock.description", "/testforblock <position> <tileName>") {}

std::vector<CommandOverloadData> TestForBlockCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makeTypedParameter("position", CommandParamType::BlockPosition),
                            makeTypedParameter("tileName", CommandParamType::String)};
    return {overload};
}

bool TestForBlockCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = sender.getPosition();
    const Vector3i originBlock((int32_t) std::floor(origin.x), (int32_t) std::floor(origin.y),
                               (int32_t) std::floor(origin.z));

    Vector3i position;
    if (arguments.size() < 4 || !parseBlockPosition(arguments, 0, originBlock, position)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y) {
        sender.sendTranslation("commands.testforblock.outOfWorld", {});
        return false;
    }

    std::string expected = arguments[3];
    if (expected.find(':') == std::string::npos)
        expected = "minecraft:" + expected;

    const std::string actual = level->getBlockState(position.x, position.y, position.z).mName;
    const std::vector<std::string> coordinates = {std::to_string(position.x), std::to_string(position.y),
                                                  std::to_string(position.z)};

    if (actual != expected) {
        sender.sendTranslation("commands.testforblock.failed.data", coordinates);
        return false;
    }

    sender.sendTranslation("commands.testforblock.success", coordinates);
    return true;
}
