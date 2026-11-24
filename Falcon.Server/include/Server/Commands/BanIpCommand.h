#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class BanIpCommand : public Command {
public:
    explicit BanIpCommand(ServerNetworkHandler &handler);

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
