#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class PardonIpCommand : public Command {
public:
    explicit PardonIpCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    CommandPermission getRequiredPermission() const override { return CommandPermission::Owner; }

private:
    ServerNetworkHandler &mHandler;
};
