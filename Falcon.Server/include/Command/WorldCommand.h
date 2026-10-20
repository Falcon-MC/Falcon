#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class WorldCommand : public Command {
public:
    explicit WorldCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    CommandPermission getRequiredPermission() const override;

private:
    bool _list(CommandOrigin &sender);

    bool _create(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _load(CommandOrigin &sender, const std::string &name);

    bool _unload(CommandOrigin &sender, const std::string &name);

    bool _teleport(CommandOrigin &sender, const std::vector<std::string> &arguments);

    ServerNetworkHandler &mHandler;
};
