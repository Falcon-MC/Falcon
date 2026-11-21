#pragma once

#include "Server/Commands/Command.h"

class ServerNetworkHandler;

class ReplaceItemCommand : public Command {
public:
    explicit ReplaceItemCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    // the whole line: the components json keeps its quotes
    size_t getRawArgumentIndex() const override {
        return 0;
    }

private:
    ServerNetworkHandler &mHandler;
};
