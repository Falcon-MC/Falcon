#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class StatusCommand : public Command {
public:
    explicit StatusCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

private:
    ServerNetworkHandler &mHandler;
};
