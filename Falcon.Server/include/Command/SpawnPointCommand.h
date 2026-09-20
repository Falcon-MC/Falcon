#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class SpawnPointCommand : public Command {
public:
    SpawnPointCommand(ServerNetworkHandler &handler, bool clear);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
    bool mClear;
};
