#include "Command/SpawnPointCommand.h"

#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <string>

SpawnPointCommand::SpawnPointCommand(ServerNetworkHandler &handler, bool clear)
        : Command(clear ? "clearspawnpoint" : "spawnpoint",
                  clear ? "commands.clearspawnpoint.description" : "commands.spawnpoint.description",
                  clear ? "/clearspawnpoint [player]" : "/spawnpoint [player] [x y z]"),
          mHandler(handler), mClear(clear) {}

std::vector<CommandOverloadData> SpawnPointCommand::getOverloads() const {
    CommandParamData player = makePlayerParameter("player", mHandler.getPlayerNames());
    player.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(player);

    if (mClear)
        return {overload};

    CommandParamData position;
    position.mName = "spawnPos";
    position.mOptional = true;
    position.mHasType = true;
    position.mType = CommandParamType::BlockPosition;

    overload.mParameters.push_back(position);
    return {overload};
}

bool SpawnPointCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const bool positionOnly = !mClear && arguments.size() == 3;
    std::vector<ServerPlayer *> targets;

    if (arguments.empty() || positionOnly) {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        targets.push_back(self);
    } else {
        targets = mHandler.resolveTargets(sender, arguments[0]);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    std::string names;
    for (const ServerPlayer *target: targets) {
        if (!names.empty())
            names += ", ";
        names += target->getName();
    }

    if (mClear) {
        for (ServerPlayer *target: targets)
            target->clearSpawnPoint();

        if (targets.size() == 1)
            sender.sendTranslation("commands.clearspawnpoint.success.single", {names});
        else
            sender.sendTranslation("commands.clearspawnpoint.success.multiple", {names});

        return true;
    }

    const bool explicitPosition = arguments.size() == 4 || positionOnly;
    if (!arguments.empty() && arguments.size() != 1 && !explicitPosition) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const size_t positionIndex = positionOnly ? 0 : 1;

    for (ServerPlayer *target: targets) {
        const Vector3f position = target->getPosition();
        Vector3i spawn((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                       (int32_t) std::floor(position.z));

        if (explicitPosition && !parseBlockPosition(arguments, positionIndex, spawn, spawn)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        Level &level = mHandler.getLevelFor(*target);
        if (spawn.y < level.getMinY() || spawn.y > level.getMaxY()) {
            sender.sendTranslation("commands.setblock.outOfWorld", {});
            return false;
        }

        target->setSpawnPoint(spawn);

        if (targets.size() == 1) {
            sender.sendTranslation("commands.spawnpoint.success.single",
                                   {target->getName(), std::to_string(spawn.x), std::to_string(spawn.y),
                                    std::to_string(spawn.z)});
        }
    }

    if (targets.size() > 1)
        sender.sendTranslation("commands.spawnpoint.success.multiple.generic", {names});

    return true;
}
