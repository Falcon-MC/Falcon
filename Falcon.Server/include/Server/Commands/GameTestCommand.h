#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

// GameTestCommand
class GameTestCommand : public Command {
public:
    explicit GameTestCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
