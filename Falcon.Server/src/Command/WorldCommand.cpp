#include "Command/WorldCommand.h"

#include "Actor/ServerPlayer.h"
#include "Level/Generator/Overworld/OverworldGenerator.h"
#include "Level/WorldManager.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>

namespace {
    std::string joinNames(const std::vector<std::string> &names) {
        std::string joined;
        for (const std::string &name: names) {
            if (!joined.empty())
                joined += ", ";
            joined += name;
        }
        return joined;
    }
}

WorldCommand::WorldCommand(ServerNetworkHandler &handler)
        : Command("world", "Manages the loaded worlds", "/world <list|create|load|unload|tp> ..."),
          mHandler(handler) {}

CommandPermission WorldCommand::getRequiredPermission() const {
    return CommandPermission::Admin;
}

std::vector<CommandOverloadData> WorldCommand::getOverloads() const {
    CommandOverloadData list;
    list.mParameters = {makeEnumParameter("action", "WorldList", {"list"})};

    CommandOverloadData create;
    create.mParameters = {makeEnumParameter("action", "WorldCreate", {"create"}),
                          makeTypedParameter("name", CommandParamType::String),
                          makeTypedParameter("seed", CommandParamType::String, true)};

    CommandOverloadData manage;
    manage.mParameters = {makeEnumParameter("action", "WorldManage", {"load", "unload"}),
                          makeTypedParameter("name", CommandParamType::String)};

    CommandOverloadData teleport;
    teleport.mParameters = {makeEnumParameter("action", "WorldTeleport", {"tp"}),
                            makeTypedParameter("name", CommandParamType::String),
                            makeTypedParameter("player", CommandParamType::Target, true)};

    return {list, create, manage, teleport};
}

bool WorldCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::string action = arguments.empty() ? std::string() : arguments[0];
    if (action == "list")
        return _list(sender);

    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (action == "create")
        return _create(sender, arguments);
    if (action == "load")
        return _load(sender, arguments[1]);
    if (action == "unload")
        return _unload(sender, arguments[1]);
    if (action == "tp")
        return _teleport(sender, arguments);

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

bool WorldCommand::_list(CommandOrigin &sender) {
    std::vector<std::string> loaded;
    for (World *world: mHandler.getWorlds().getWorlds())
        loaded.push_back(world->getName());

    std::vector<std::string> unloaded;
    for (const std::string &name: WorldManager::listOnDisk()) {
        if (mHandler.getWorlds().find(name) == nullptr)
            unloaded.push_back(name);
    }

    sender.sendLocalized("falcon.commands.world.list", {std::to_string(loaded.size()), joinNames(loaded)});
    if (!unloaded.empty())
        sender.sendLocalized("falcon.commands.world.available", {joinNames(unloaded)});
    return true;
}

bool WorldCommand::_create(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::string &name = arguments[1];
    if (!WorldManager::isValidName(name)) {
        sender.sendLocalized("falcon.commands.world.invalidName", {name});
        return false;
    }

    if (WorldManager::exists(name) || mHandler.getWorlds().find(name) != nullptr) {
        sender.sendLocalized("falcon.commands.world.exists", {name});
        return false;
    }

    const int64_t seed = arguments.size() > 2 ? OverworldGenerator::parseSeed(arguments[2])
                                              : ServerNetworkHandler::randomSeed();
    if (mHandler.loadWorld(name, true, seed) == nullptr) {
        sender.sendLocalized("falcon.commands.world.invalidName", {name});
        return false;
    }

    sender.sendLocalized("falcon.commands.world.created", {name});
    return true;
}

bool WorldCommand::_load(CommandOrigin &sender, const std::string &name) {
    if (mHandler.getWorlds().find(name) != nullptr) {
        sender.sendLocalized("falcon.commands.world.alreadyLoaded", {name});
        return false;
    }

    if (mHandler.loadWorld(name, false, ServerNetworkHandler::randomSeed()) == nullptr) {
        sender.sendLocalized("falcon.commands.world.notFound", {name});
        return false;
    }

    sender.sendLocalized("falcon.commands.world.loaded", {name});
    return true;
}

bool WorldCommand::_unload(CommandOrigin &sender, const std::string &name) {
    std::string error;
    if (!mHandler.unloadWorld(name, error)) {
        sender.sendLocalized(error, {name});
        return false;
    }

    sender.sendLocalized("falcon.commands.world.unloaded", {name});
    return true;
}

bool WorldCommand::_teleport(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    World *world = mHandler.getWorlds().find(arguments[1]);
    if (world == nullptr) {
        sender.sendLocalized("falcon.commands.world.notLoaded", {arguments[1]});
        return false;
    }

    std::vector<ServerPlayer *> targets;
    if (arguments.size() > 2) {
        targets = mHandler.resolveTargets(sender, arguments[2]);
    } else if (ServerPlayer *self = sender.asPlayer()) {
        targets.push_back(self);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    Level &overworld = world->getOverworld();
    const Vector3f spawn = overworld.getSpawnPositionForPlayer();
    for (ServerPlayer *target: targets) {
        if (target->isIn(overworld))
            target->teleport(mHandler, spawn);
        else
            mHandler.changePlayerLevel(*target, overworld, spawn);

        sender.sendLocalized("falcon.commands.world.teleported", {target->getName(), world->getName()});
    }
    return true;
}
