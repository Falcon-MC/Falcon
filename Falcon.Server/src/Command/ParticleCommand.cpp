#include "Command/ParticleCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

ParticleCommand::ParticleCommand(ServerNetworkHandler &handler)
        : Command("particle", "commands.particle.description", "/particle <effect> [position]"), mHandler(handler) {}

std::vector<CommandOverloadData> ParticleCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makeTypedParameter("effect", CommandParamType::String),
                            makeTypedParameter("position", CommandParamType::Position, true)};
    return {overload};
}

bool ParticleCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = sender.getPosition();
    Vector3f position = origin;
    if (arguments.size() > 3
        && (!parseCoordinate(arguments[1], origin.x, position.x)
            || !parseCoordinate(arguments[2], origin.y, position.y)
            || !parseCoordinate(arguments[3], origin.z, position.z))) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    mHandler.spawnParticleEffect(*level, arguments[0], position);
    sender.sendTranslation("commands.particle.success", {arguments[0], "1"});
    return true;
}
