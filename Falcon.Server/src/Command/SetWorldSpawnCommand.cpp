#include "Command/SetWorldSpawnCommand.h"

#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/SetSpawnPositionPacket.h"

#include <cmath>
#include <string>

SetWorldSpawnCommand::SetWorldSpawnCommand(ServerNetworkHandler &handler)
        : Command("setworldspawn", "commands.setworldspawn.description", "/setworldspawn [x y z]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> SetWorldSpawnCommand::getOverloads() const {
    CommandParamData position;
    position.mName = "spawnPoint";
    position.mOptional = true;
    position.mHasType = true;
    position.mType = CommandParamType::BlockPosition;

    CommandOverloadData overload;
    overload.mParameters.push_back(position);

    return {overload};
}

bool SetWorldSpawnCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (!arguments.empty() && arguments.size() != 3) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    ServerPlayer *self = sender.asPlayer();
    if (self == nullptr) {
        sender.sendTranslation("commands.generic.targetNotPlayer", {});
        return false;
    }

    Level &level = mHandler.getLevelFor(*self);
    if (level.getDimensionType() != DimensionType::Overworld) {
        sender.sendTranslation("commands.setworldspawn.wrongDimension", {});
        return false;
    }

    const Vector3f position = self->getPosition();
    Vector3i spawn((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                   (int32_t) std::floor(position.z));

    if (!arguments.empty() && !parseBlockPosition(arguments, 0, spawn, spawn)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (spawn.y < level.getMinY() || spawn.y > level.getMaxY()) {
        sender.sendTranslation("commands.setworldspawn.wrongDimension", {});
        return false;
    }

    level.setSpawnPosition(spawn);
    level.saveLevelDat();

    SetSpawnPositionPacket packet;
    packet.mSpawnType = SetSpawnPositionPacket::Type::WorldSpawn;
    packet.mBlockPosition = spawn;
    packet.mDimensionId = level.getDimensionId();
    packet.mSpawnPosition = spawn;

    for (auto &entry: mHandler.getPlayers()) {
        if (entry.second.isSpawned())
            mHandler.getNetworkHandler().send(entry.first, packet, mHandler.getCodecContext());
    }

    sender.sendTranslation("commands.setworldspawn.success",
                           {std::to_string(spawn.x), std::to_string(spawn.y), std::to_string(spawn.z)});
    return true;
}
