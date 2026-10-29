#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class AboutCommand : public Command {
public:
    explicit AboutCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
