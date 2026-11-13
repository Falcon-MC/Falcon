#include "Server/Commands/GameTestCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Actor.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

#include <cmath>
#include <cstdlib>

namespace {
    CommandParamData mode(const char *value, const char *enumName) {
        return Command::makeEnumParameter("mode", enumName, {value});
    }

    CommandParamData softEnum(const std::string &name, const char *enumName, std::vector<std::string> values,
                              bool optional) {
        CommandParamData parameter = Command::makeEnumParameter(name, enumName, std::move(values), optional);
        parameter.mEnumData.mIsSoft = true;
        return parameter;
    }

    bool parseInt(const std::string &text, int32_t &out) {
        if (text.empty())
            return false;
        char *end = nullptr;
        const long value = std::strtol(text.c_str(), &end, 10);
        if (end == nullptr || *end != '\0')
            return false;
        out = (int32_t) value;
        return true;
    }

    bool parseBool(const std::string &text, bool &out) {
        if (text == "true" || text == "false") {
            out = text == "true";
            return true;
        }
        return false;
    }

    void answer(CommandOrigin &sender, const GameTestRunner::Outcome &outcome) {
        if (!outcome.mKey.empty())
            sender.sendTranslation(outcome.mKey, outcome.mParameters);
    }
}

GameTestCommand::GameTestCommand(ServerNetworkHandler &handler)
        : Command("gametest", "gameTest.description", "/gametest <mode>"), mHandler(handler) {}

CommandPermission GameTestCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

// the game's overloads, each mode with its own one value enum; names and tags are soft enums
std::vector<CommandOverloadData> GameTestCommand::getOverloads() const {
    GameTestRunner &runner = mHandler.getScriptEngine().gameTests();
    const std::vector<std::string> names = runner.testNames();
    const std::vector<std::string> tags = runner.knownTags();
    const CommandParamData rotation = makeTypedParameter("rotationStep", CommandParamType::Int, true);

    std::vector<CommandOverloadData> overloads;
    CommandOverloadData run;
    run.mParameters = {mode("run", "GameTestModeRun"), softEnum("testName", "GameTestName", names, false), rotation};
    overloads.push_back(run);

    CommandOverloadData repeat;
    repeat.mParameters = {mode("run", "GameTestModeRun"), softEnum("testName", "GameTestName", names, false),
                          makeEnumParameter("stopOnFailure", "Boolean", {"true", "false"}),
                          makeTypedParameter("repeatCount", CommandParamType::Int), rotation};
    overloads.push_back(repeat);

    for (const auto &set: {std::make_pair("runset", "GameTestModeRunSet"),
                           std::make_pair("runsetuntilfail", "GameTestModeRunSetUntilFail")}) {
        CommandOverloadData overload;
        overload.mParameters = {mode(set.first, set.second), softEnum("tag", "GameTestTag", tags, true), rotation};
        overloads.push_back(overload);
    }

    CommandOverloadData create;
    create.mParameters = {mode("create", "GameTestModeCreateTest"),
                          makeTypedParameter("testName", CommandParamType::String),
                          makeTypedParameter("width", CommandParamType::Int, true),
                          makeTypedParameter("height", CommandParamType::Int, true),
                          makeTypedParameter("depth", CommandParamType::Int, true)};
    overloads.push_back(create);

    for (const auto &single:
         {std::make_pair("clearall", "GameTestModeClearAll"), std::make_pair("pos", "GameTestModeShowPosition"),
          std::make_pair("runthis", "GameTestModeRunThis"), std::make_pair("runthese", "GameTestRunNearbyTests"),
          std::make_pair("stopall", "GameTestStopTests")}) {
        CommandOverloadData overload;
        overload.mParameters = {mode(single.first, single.second)};
        overloads.push_back(overload);
    }
    return overloads;
}

bool GameTestCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendError("CommandOrigin did not have a BlockSource");
        return false;
    }
    GameTestRunner &runner = mHandler.getScriptEngine().gameTests();
    const Vector3f position = sender.getPosition();
    const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                          (int32_t) std::floor(position.z));
    const std::string &modeName = arguments[0];

    // the rotation step counts only below 4
    const auto rotationAt = [&arguments](size_t index) {
        int32_t step = 0;
        if (index < arguments.size() && parseInt(arguments[index], step) && step >= 0 && step < 4)
            return step;
        return 0;
    };

    GameTestRunner::Outcome outcome;
    if (modeName == "run") {
        if (arguments.size() < 2) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        bool stopOnFailure = false;
        int32_t repeatCount = 1;
        int32_t rotation = rotationAt(2);
        if (arguments.size() >= 4 && parseBool(arguments[2], stopOnFailure)) {
            if (!parseInt(arguments[3], repeatCount)) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }
            rotation = rotationAt(4);
        }
        outcome = runner.run(*level, origin, arguments[1], rotation, stopOnFailure, repeatCount);
    } else if (modeName == "runthis") {
        outcome = runner.runThis(*level, origin);
    } else if (modeName == "runthese") {
        outcome = runner.runThese(*level, origin);
    } else if (modeName == "runset" || modeName == "runsetuntilfail") {
        int32_t step = 0;
        std::string tag;
        size_t rotationIndex = 1;
        if (arguments.size() >= 2 && !parseInt(arguments[1], step)) {
            tag = arguments[1];
            rotationIndex = 2;
        }
        outcome = runner.runSet(*level, origin, tag, rotationAt(rotationIndex), modeName == "runsetuntilfail");
    } else if (modeName == "create") {
        if (arguments.size() < 2) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        int32_t width = GameTestRunner::DEFAULT_CREATE_SIZE, height = 0, depth = 0;
        if ((arguments.size() >= 3 && !parseInt(arguments[2], width))
            || (arguments.size() >= 4 && !parseInt(arguments[3], height))
            || (arguments.size() >= 5 && !parseInt(arguments[4], depth))) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        outcome = runner.create(*level, origin, arguments[1], width, height, depth);
    } else if (modeName == "clearall") {
        outcome = runner.clearAll(*level);
    } else if (modeName == "stopall") {
        outcome = runner.stopAll();
    } else if (modeName == "pos") {
        Actor *entity = sender.getEntity();
        if (entity == nullptr) {
            sender.sendError("Command was not executed from an Actor");
            return false;
        }
        // the eyes, ten blocks along the view
        const Vector3f &rotation = entity->getRotation();
        const float pitch = rotation.x * 0.017453292f, yaw = rotation.y * 0.017453292f;
        const Vector3f direction(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        const Vector3f &feet = entity->getPosition();
        ScriptRaycast::BlockOptions ray;
        ray.mMaxDistance = 10.0f;
        ScriptRaycast::BlockHit hit;
        if (!ScriptRaycast::castBlocks(*level,
                                       Vector3f(feet.x, feet.y + PlacementOrientation::PLAYER_EYE_HEIGHT, feet.z),
                                       direction, ray, hit)) {
            sender.sendTranslation("gameTest.error.cannotFindLookedAtBlock", {});
            return false;
        }
        outcome = runner.position(*level, hit.mBlock);
    } else {
        sender.sendTranslation("gameTest.error.unknownCommandMode", {});
        return false;
    }
    answer(sender, outcome);
    return outcome.mSuccess;
}
