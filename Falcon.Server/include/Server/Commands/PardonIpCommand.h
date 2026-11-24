#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class PardonIpCommand : public Command {
public:
    explicit PardonIpCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    bool managesServer() const override {
        return true;
    }

    std::vector<CommandOverloadData> getOverloads() const override;

    CommandPermission getRequiredPermission() const override {
        return CommandPermission::GameDirectors;
    }

private:
    ServerNetworkHandler &mHandler;
};
