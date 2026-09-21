#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class StopCommand : public Command {
public:
    explicit StopCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override { return CommandPermission::Owner; }

private:
    ServerNetworkHandler &mHandler;
};
