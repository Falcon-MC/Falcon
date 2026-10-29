#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class EffectCommand final : public Command {
public:
    explicit EffectCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
