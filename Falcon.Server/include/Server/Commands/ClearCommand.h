#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;
class ServerPlayer;

class ClearCommand final : public Command {
public:
    explicit ClearCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    // An empty identifier clears everything, a maxCount of 0 only counts and -1 takes every match.
    static int32_t clearMatching(ServerPlayer &target, const std::string &identifier, int32_t data,
                                 int32_t maxCount);

    ServerNetworkHandler &mHandler;
};
