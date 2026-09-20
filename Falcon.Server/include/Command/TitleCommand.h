#pragma once

#include "Command/Command.h"

class ServerPlayer;
class ServerNetworkHandler;

class TitleCommand : public Command {
public:
    TitleCommand(ServerNetworkHandler &handler, bool raw);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    bool applyToTarget(ServerPlayer &target, const std::string &action,
                       const std::vector<std::string> &arguments);

    ServerNetworkHandler &mHandler;
    bool mRaw;
};
