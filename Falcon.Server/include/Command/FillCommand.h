#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class FillCommand : public Command {
public:
    explicit FillCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    static constexpr int32_t MAX_BLOCKS = 32768;

    ServerNetworkHandler &mHandler;
};
