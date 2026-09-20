#pragma once

#include "Command/Command.h"
#include "Server/PropertiesSettings.h"

class ServerNetworkHandler;

class DifficultyCommand : public Command {
public:
    explicit DifficultyCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    static bool parseDifficulty(const std::string &value, Difficulty &out);

    ServerNetworkHandler &mHandler;
};
