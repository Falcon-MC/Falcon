#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class DeopCommand : public Command {
public:
    explicit DeopCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
