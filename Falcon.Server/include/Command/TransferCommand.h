#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class TransferCommand : public Command {
public:
    explicit TransferCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
