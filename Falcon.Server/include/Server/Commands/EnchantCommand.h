#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class EnchantCommand : public Command {
public:
    explicit EnchantCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    static bool _parseLevel(const std::string &value, int32_t &out);

    ServerNetworkHandler &mHandler;
};
