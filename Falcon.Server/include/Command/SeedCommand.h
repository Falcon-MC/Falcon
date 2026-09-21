#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class SeedCommand : public Command {
public:
    explicit SeedCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

private:
    ServerNetworkHandler &mHandler;
};
