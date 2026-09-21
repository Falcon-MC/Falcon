#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class HelpCommand : public Command {
public:
    explicit HelpCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    CommandPermission getRequiredPermission() const override { return CommandPermission::Any; }

private:
    ServerNetworkHandler &mHandler;
};
