#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class LocateCommand : public Command {
public:
    explicit LocateCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    bool _locateBiome(CommandOrigin &sender, const std::vector<std::string> &arguments);

    ServerNetworkHandler &mHandler;
};
