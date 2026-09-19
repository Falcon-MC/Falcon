#pragma once

#include "Command/Command.h"
#include "Core/Math/Vector3f.h"

class ServerNetworkHandler;
class ServerPlayer;

class TeleportCommand : public Command {
public:
    explicit TeleportCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    static bool parseCoordinate(const std::string &value, float origin, float &out);

    static bool parsePosition(const std::vector<std::string> &arguments, size_t first, const Vector3f &origin,
                              Vector3f &out);

    bool teleportToPlayer(CommandOrigin &sender, const std::vector<ServerPlayer *> &victims,
                          const std::string &destination);

    bool teleportToPosition(CommandOrigin &sender, const std::vector<ServerPlayer *> &victims,
                            const std::vector<std::string> &arguments, size_t first);

    ServerNetworkHandler &mHandler;
};
