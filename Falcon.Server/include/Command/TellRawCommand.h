#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class TellRawCommand : public Command {
public:
    explicit TellRawCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    size_t getRawArgumentIndex() const override { return 1; }

private:
    ServerNetworkHandler &mHandler;
};
