#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class DamageCommand : public Command {
public:
    explicit DamageCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    static const char *findDeathMessageKey(const std::string &cause);

private:
    ServerNetworkHandler &mHandler;
};
