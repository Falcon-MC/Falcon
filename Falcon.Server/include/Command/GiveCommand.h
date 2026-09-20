#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class GiveCommand : public Command {
public:
    explicit GiveCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    static bool _parseCount(const std::string &value, int &out);

    static bool _parseData(const std::string &value, int &out);

    ServerNetworkHandler &mHandler;
};
