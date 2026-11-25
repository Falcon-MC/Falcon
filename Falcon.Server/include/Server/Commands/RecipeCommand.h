#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

// /recipe give|take <player> <recipe|*>: the recipe book, by recipe id
class RecipeCommand : public Command {
public:
    explicit RecipeCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    ServerNetworkHandler &mHandler;
};
