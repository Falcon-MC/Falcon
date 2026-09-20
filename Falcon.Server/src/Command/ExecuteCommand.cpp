#include "Command/ExecuteCommand.h"

#include "Actor/ServerPlayer.h"
#include "Command/CommandMap.h"
#include "Command/ExecuteCommandOrigin.h"
#include "Command/SetBlockCommand.h"
#include "Core/Math/MathConstants.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
    const std::vector<std::string> SUBCOMMANDS = {"align", "anchored", "as", "at", "facing", "if",
                                                  "in", "positioned", "rotated", "run", "unless"};

    const float PLAYER_EYE_HEIGHT = 1.62f;

    bool parseFloat(const std::string &value, float &out) {
        char *end = nullptr;
        errno = 0;
        const float parsed = std::strtof(value.c_str(), &end);

        if (value.empty() || end == value.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
            return false;

        out = parsed;
        return true;
    }

    bool parseRotation(const std::string &value, float origin, float &out) {
        const bool relative = !value.empty() && value[0] == '~';
        const std::string number = relative ? value.substr(1) : value;

        float parsed = 0.0f;
        if (!number.empty() && !parseFloat(number, parsed))
            return false;

        if (number.empty() && !relative)
            return false;

        out = relative ? origin + parsed : parsed;
        return true;
    }

    Vector3f rotationTowards(const Vector3f &from, const Vector3f &to) {
        const float deltaX = to.x - from.x;
        const float deltaY = to.y - from.y;
        const float deltaZ = to.z - from.z;

        const float horizontal = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        const float yaw = (float) (std::atan2(deltaZ, deltaX) * 180.0 / MathConstants::PI) - 90.0f;
        const float pitch = (float) (-std::atan2(deltaY, horizontal) * 180.0 / MathConstants::PI);

        return Vector3f(pitch, yaw, 0.0f);
    }

    Vector3i toBlockPosition(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

ExecuteCommand::ExecuteCommand(ServerNetworkHandler &handler)
        : Command("execute", "commands.execute.description", "/execute <subcommand> ... run <command>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> ExecuteCommand::getOverloads() const {
    CommandParamData subcommand;
    subcommand.mName = "subcommand";
    subcommand.mHasEnumData = true;
    subcommand.mEnumData.mName = "ExecuteSubcommand";
    subcommand.mEnumData.mValues = SUBCOMMANDS;

    CommandParamData rest;
    rest.mName = "args";
    rest.mHasType = true;
    rest.mType = CommandParamType::RawText;

    CommandOverloadData overload;
    overload.mParameters.push_back(subcommand);
    overload.mParameters.push_back(rest);

    return {overload};
}

std::vector<ServerPlayer *> ExecuteCommand::resolveTargets(CommandOrigin &sender, const Context &context,
                                                           const std::string &selector) {
    ExecuteCommandOrigin origin(sender, context.mExecutor, context.mPosition, context.mRotation, context.mLevel);
    return mHandler.resolveTargets(origin, selector);
}

Level &ExecuteCommand::resolveLevel(const Context &context) const {
    return context.mLevel == nullptr ? mHandler.getLevel() : *context.mLevel;
}

bool ExecuteCommand::runChain(CommandOrigin &sender, const Context &context,
                              const std::vector<std::string> &arguments, size_t index, int32_t &successes) {
    Context copy = context;
    return run(sender, copy, arguments, index, successes);
}

bool ExecuteCommand::testBlock(CommandOrigin &sender, const Context &context,
                               const std::vector<std::string> &arguments, size_t index, size_t &next,
                               bool &matched) {
    if (arguments.size() < index + 4) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Level &level = resolveLevel(context);
    Vector3i position = toBlockPosition(context.mPosition);

    if (!parseBlockPosition(arguments, index, position, position)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    BlockState expected;
    if (!SetBlockCommand::resolveBlock(arguments[index + 3], expected)) {
        sender.sendTranslation("commands.setblock.notFound", {arguments[index + 3]});
        return false;
    }

    if (position.y < level.getMinY() || position.y > level.getMaxY()) {
        sender.sendTranslation("commands.testforblock.outOfWorld", {});
        return false;
    }

    matched = level.getBlockState(position.x, position.y, position.z).mName == expected.mName;
    next = index + 4;
    return true;
}

bool ExecuteCommand::testBlocks(CommandOrigin &sender, const Context &context,
                                const std::vector<std::string> &arguments, size_t index, size_t &next,
                                bool &matched, int32_t &count) {
    if (arguments.size() < index + 9) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Level &level = resolveLevel(context);
    const Vector3i origin = toBlockPosition(context.mPosition);

    Vector3i begin = origin;
    Vector3i end = origin;
    Vector3i destination = origin;

    if (!parseBlockPosition(arguments, index, origin, begin)
        || !parseBlockPosition(arguments, index + 3, origin, end)
        || !parseBlockPosition(arguments, index + 6, origin, destination)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    next = index + 9;

    bool masked = false;
    if (arguments.size() > next && (arguments[next] == "all" || arguments[next] == "masked")) {
        masked = arguments[next] == "masked";
        ++next;
    }

    const Vector3i minimum(std::min(begin.x, end.x), std::min(begin.y, end.y), std::min(begin.z, end.z));
    const Vector3i maximum(std::max(begin.x, end.x), std::max(begin.y, end.y), std::max(begin.z, end.z));

    const int64_t width = (int64_t) maximum.x - minimum.x + 1;
    const int64_t height = (int64_t) maximum.y - minimum.y + 1;
    const int64_t depth = (int64_t) maximum.z - minimum.z + 1;
    const int64_t volume = width * height * depth;

    if (volume > MAX_COMPARED_BLOCKS) {
        sender.sendTranslation("commands.execute.ifUnlessBlocks.tooManyBlocks",
                               {std::to_string(MAX_COMPARED_BLOCKS), std::to_string(volume)});
        return false;
    }

    if (minimum.y < level.getMinY() || maximum.y > level.getMaxY()
        || destination.y < level.getMinY() || destination.y + height - 1 > level.getMaxY()) {
        sender.sendTranslation("commands.testforblock.outOfWorld", {});
        return false;
    }

    matched = true;
    count = 0;

    for (int32_t x = minimum.x; x <= maximum.x && matched; ++x) {
        for (int32_t y = minimum.y; y <= maximum.y && matched; ++y) {
            for (int32_t z = minimum.z; z <= maximum.z && matched; ++z) {
                const BlockState source = level.getBlockState(x, y, z);
                const BlockState target = level.getBlockState(destination.x + (x - minimum.x),
                                                              destination.y + (y - minimum.y),
                                                              destination.z + (z - minimum.z));

                if (source.mName == target.mName) {
                    ++count;
                    continue;
                }

                if (masked && source.mName == "minecraft:air")
                    continue;

                matched = false;
            }
        }
    }

    return true;
}

bool ExecuteCommand::run(CommandOrigin &sender, Context context, const std::vector<std::string> &arguments,
                         size_t index, int32_t &successes) {
    if (index >= arguments.size()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string subcommand = arguments[index];

    if (subcommand == "run") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        if (mExecuted >= MAX_COMMANDS)
            return false;

        ++mExecuted;

        ExecuteCommandOrigin origin(sender, context.mExecutor, context.mPosition, context.mRotation,
                                    context.mLevel);

        const std::string chained = joinArguments(arguments, index + 1);

        if (!mHandler.getCommands().dispatch(origin, chained)) {
            sender.sendTranslation("commands.execute.failed", {chained, origin.getSenderName()});
            return false;
        }

        ++successes;
        return true;
    }

    if (subcommand == "as" || subcommand == "at") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        const std::vector<ServerPlayer *> targets = resolveTargets(sender, context, arguments[index + 1]);
        if (targets.empty()) {
            sender.sendTranslation("commands.generic.noTargetMatch", {});
            return false;
        }

        bool result = false;
        for (ServerPlayer *target: targets) {
            Context derived = context;

            if (subcommand == "as") {
                derived.mExecutor = target;
            } else {
                derived.mPosition = target->getPosition();
                derived.mRotation = target->getRotation();
                derived.mLevel = &mHandler.getLevelFor(*target);
            }

            if (runChain(sender, derived, arguments, index + 2, successes))
                result = true;
        }

        return result;
    }

    if (subcommand == "in") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        std::string name = arguments[index + 1];
        if (name.find(':') != std::string::npos)
            name = name.substr(name.find(':') + 1);

        if (name == "overworld")
            context.mLevel = &mHandler.getDimension(DimensionType::Overworld);
        else if (name == "nether")
            context.mLevel = &mHandler.getDimension(DimensionType::Nether);
        else if (name == "the_end" || name == "end")
            context.mLevel = &mHandler.getDimension(DimensionType::TheEnd);
        else {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        return runChain(sender, context, arguments, index + 2, successes);
    }

    if (subcommand == "positioned") {
        if (index + 1 < arguments.size() && arguments[index + 1] == "as") {
            if (index + 2 >= arguments.size()) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }

            const std::vector<ServerPlayer *> targets = resolveTargets(sender, context, arguments[index + 2]);
            if (targets.empty()) {
                sender.sendTranslation("commands.generic.noTargetMatch", {});
                return false;
            }

            bool result = false;
            for (ServerPlayer *target: targets) {
                Context derived = context;
                derived.mPosition = target->getPosition();

                if (runChain(sender, derived, arguments, index + 3, successes))
                    result = true;
            }

            return result;
        }

        Vector3f position = context.mPosition;
        if (arguments.size() < index + 4
            || !parseCoordinate(arguments[index + 1], context.mPosition.x, position.x)
            || !parseCoordinate(arguments[index + 2], context.mPosition.y, position.y)
            || !parseCoordinate(arguments[index + 3], context.mPosition.z, position.z)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        context.mPosition = position;
        return runChain(sender, context, arguments, index + 4, successes);
    }

    if (subcommand == "rotated") {
        if (index + 1 < arguments.size() && arguments[index + 1] == "as") {
            if (index + 2 >= arguments.size()) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }

            const std::vector<ServerPlayer *> targets = resolveTargets(sender, context, arguments[index + 2]);
            if (targets.empty()) {
                sender.sendTranslation("commands.generic.noTargetMatch", {});
                return false;
            }

            bool result = false;
            for (ServerPlayer *target: targets) {
                Context derived = context;
                derived.mRotation = target->getRotation();

                if (runChain(sender, derived, arguments, index + 3, successes))
                    result = true;
            }

            return result;
        }

        Vector3f rotation = context.mRotation;
        if (arguments.size() < index + 3
            || !parseRotation(arguments[index + 1], context.mRotation.y, rotation.y)
            || !parseRotation(arguments[index + 2], context.mRotation.x, rotation.x)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        context.mRotation = rotation;
        return runChain(sender, context, arguments, index + 3, successes);
    }

    if (subcommand == "facing") {
        if (index + 1 < arguments.size() && arguments[index + 1] == "entity") {
            if (index + 3 >= arguments.size()) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }

            const std::vector<ServerPlayer *> targets = resolveTargets(sender, context, arguments[index + 2]);
            if (targets.empty()) {
                sender.sendTranslation("commands.generic.noTargetMatch", {});
                return false;
            }

            const bool eyes = arguments[index + 3] == "eyes";

            bool result = false;
            for (ServerPlayer *target: targets) {
                Vector3f destination = target->getPosition();
                if (eyes)
                    destination.y += PLAYER_EYE_HEIGHT;

                Context derived = context;
                derived.mRotation = rotationTowards(context.mPosition, destination);

                if (runChain(sender, derived, arguments, index + 4, successes))
                    result = true;
            }

            return result;
        }

        Vector3f destination = context.mPosition;
        if (arguments.size() < index + 4
            || !parseCoordinate(arguments[index + 1], context.mPosition.x, destination.x)
            || !parseCoordinate(arguments[index + 2], context.mPosition.y, destination.y)
            || !parseCoordinate(arguments[index + 3], context.mPosition.z, destination.z)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        context.mRotation = rotationTowards(context.mPosition, destination);
        return runChain(sender, context, arguments, index + 4, successes);
    }

    if (subcommand == "align") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        const std::string axes = arguments[index + 1];
        if (axes.empty() || axes.size() > 3) {
            sender.sendTranslation("commands.execute.align.invalidInput", {});
            return false;
        }

        std::string seen;
        for (char axis: axes) {
            if ((axis != 'x' && axis != 'y' && axis != 'z') || seen.find(axis) != std::string::npos) {
                sender.sendTranslation("commands.execute.align.invalidInput", {});
                return false;
            }

            seen.push_back(axis);

            if (axis == 'x')
                context.mPosition.x = std::floor(context.mPosition.x);
            else if (axis == 'y')
                context.mPosition.y = std::floor(context.mPosition.y);
            else
                context.mPosition.z = std::floor(context.mPosition.z);
        }

        return runChain(sender, context, arguments, index + 2, successes);
    }

    if (subcommand == "anchored") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        if (arguments[index + 1] == "eyes")
            context.mPosition.y += PLAYER_EYE_HEIGHT;
        else if (arguments[index + 1] != "feet") {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        return runChain(sender, context, arguments, index + 2, successes);
    }

    if (subcommand == "if" || subcommand == "unless") {
        if (index + 1 >= arguments.size()) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        const bool shouldMatch = subcommand == "if";
        const std::string condition = arguments[index + 1];

        bool matched = false;
        size_t next = index + 2;
        int32_t count = 0;
        bool withCount = false;

        if (condition == "block") {
            if (!testBlock(sender, context, arguments, index + 2, next, matched))
                return false;
        } else if (condition == "blocks") {
            if (!testBlocks(sender, context, arguments, index + 2, next, matched, count))
                return false;

            withCount = true;
        } else if (condition == "entity") {
            if (index + 2 >= arguments.size()) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }

            matched = !resolveTargets(sender, context, arguments[index + 2]).empty();
            next = index + 3;
        } else {
            //TODO: the score condition needs a scoreboard, which does not exist yet
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        if (matched != shouldMatch) {
            if (withCount) {
                sender.sendTranslation("commands.execute.falseConditionWithCount",
                                       {subcommand, condition, std::to_string(count)});
            } else {
                sender.sendTranslation("commands.execute.falseCondition", {subcommand, condition});
            }

            return false;
        }

        if (next >= arguments.size()) {
            if (withCount)
                sender.sendTranslation("commands.execute.trueConditionWithCount", {std::to_string(count)});
            else
                sender.sendTranslation("commands.execute.trueCondition", {});

            ++successes;
            return true;
        }

        return runChain(sender, context, arguments, next, successes);
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

bool ExecuteCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (mDepth >= MAX_DEPTH) {
        sender.sendTranslation("commands.execute.allInvocationsFailed", {joinArguments(arguments, 0)});
        return false;
    }

    if (mDepth == 0)
        mExecuted = 0;

    Context context;
    context.mExecutor = sender.asPlayer();
    context.mPosition = sender.getPosition();
    context.mRotation = sender.getRotation();
    context.mLevel = sender.getLevel();

    if (context.mLevel == nullptr && context.mExecutor != nullptr)
        context.mLevel = &mHandler.getLevelFor(*context.mExecutor);

    int32_t successes = 0;

    ++mDepth;
    const bool result = run(sender, context, arguments, 0, successes);
    --mDepth;

    return result;
}
