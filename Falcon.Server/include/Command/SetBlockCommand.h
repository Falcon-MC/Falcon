#pragma once

#include "Block/BlockState.h"
#include "Command/Command.h"

class Level;
class ServerNetworkHandler;

class SetBlockCommand : public Command {
public:
    explicit SetBlockCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    static bool resolveBlock(const std::string &name, BlockState &out);

    static bool placeBlock(ServerNetworkHandler &handler, Level &level, const Vector3i &position,
                           const BlockState &state, const std::string &mode);

private:
    ServerNetworkHandler &mHandler;
};
