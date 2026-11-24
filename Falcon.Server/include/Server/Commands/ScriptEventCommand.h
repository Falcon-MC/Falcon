#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class ScriptEventCommand : public Command {
public:
    explicit ScriptEventCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
