#include "Command/SummonCommand.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Actor/VanillaActorTable.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>

namespace {
    bool isSummonable(const std::string &identifier) {
        const char *const *identifiers = VanillaActorTable::getIdentifiers();

        for (size_t index = 0; index < VanillaActorTable::getCount(); ++index) {
            if (identifier == identifiers[index])
                return true;
        }

        return false;
    }
}

SummonCommand::SummonCommand(ServerNetworkHandler &handler)
        : Command("summon", "commands.summon.description", "/summon <entityType> [x] [y] [z]"), mHandler(handler) {}

std::vector<CommandOverloadData> SummonCommand::getOverloads() const {
    CommandParamData entityType;
    entityType.mName = "entityType";
    entityType.mHasEnumData = true;
    entityType.mEnumData.mName = "EntityType";
    entityType.mEnumData.mIsSoft = true;

    const char *const *identifiers = VanillaActorTable::getIdentifiers();
    for (size_t index = 0; index < VanillaActorTable::getCount(); ++index)
        entityType.mEnumData.mValues.push_back(identifiers[index]);

    CommandParamData position;
    position.mName = "spawnPos";
    position.mOptional = true;
    position.mHasType = true;
    position.mType = CommandParamType::Position;

    CommandOverloadData overload;
    overload.mParameters.push_back(entityType);
    overload.mParameters.push_back(position);

    return {overload};
}

bool SummonCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty() || arguments.size() == 2 || arguments.size() == 3 || arguments.size() > 4) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    ServerPlayer *self = sender.asPlayer();
    if (self == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    std::string identifier = arguments[0];
    if (identifier.find(':') == std::string::npos)
        identifier = "minecraft:" + identifier;

    if (!isSummonable(identifier)) {
        sender.sendTranslation("commands.summon.failed", {});
        return false;
    }

    Vector3f position = self->getPosition();
    if (arguments.size() == 4) {
        const Vector3f origin = position;

        if (!parseCoordinate(arguments[1], origin.x, position.x)
            || !parseCoordinate(arguments[2], origin.y, position.y)
            || !parseCoordinate(arguments[3], origin.z, position.z)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
    }

    Level &level = mHandler.getLevelFor(*self);

    if (position.y < (float) level.getMinY() || position.y > (float) level.getMaxY()) {
        sender.sendTranslation("commands.summon.outOfWorld", {});
        return false;
    }

    if (mHandler.spawnActor(level, identifier, position) == nullptr) {
        sender.sendTranslation("commands.summon.failed", {});
        return false;
    }

    sender.sendTranslation("commands.summon.success", {});
    return true;
}
