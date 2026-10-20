#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class GameModeCommand : public Command {
public:
    explicit GameModeCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    static int parseGameMode(const std::string &value);

    static const char *getGameModeName(int gameMode);

private:
    ServerNetworkHandler &mHandler;
};
