#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class MeCommand : public Command {
public:
    explicit MeCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
