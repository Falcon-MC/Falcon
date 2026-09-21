#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class DefaultGameModeCommand : public Command {
public:
    explicit DefaultGameModeCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
