#include "Server/Commands/ParticleCommand.h"

#include "Network/ServerNetworkHandler.h"

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
        sendUsageError(sender);
        return false;
    }

    Level *level = sender.getLevel();
    if (level == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    const Vector3f origin = sender.getPosition();
    Vector3f position = origin;
    if (arguments.size() > 3 && !parsePosition(arguments, 1, origin, sender.getRotation(), position)) {
        sendUsageError(sender);
        return false;
    }

    mHandler.spawnParticleEffect(*level, arguments[0], position);
    sender.sendTranslation("commands.particle.success", {arguments[0], "1"});
    return true;
}
