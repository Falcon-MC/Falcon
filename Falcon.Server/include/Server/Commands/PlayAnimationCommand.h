#pragma once

#include "Server/Commands/Command.h"

class AnimateEntityPacket;
class ServerNetworkHandler;

class PlayAnimationCommand final : public Command {
public:
    explicit PlayAnimationCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    // the animation and its options from the arguments after the target, with the game's defaults; false
    // with the argument that is no number when blend_out_time is not one
    static bool readAnimation(const std::vector<std::string> &arguments, AnimateEntityPacket &packet,
                              std::string &invalid);

private:
    ServerNetworkHandler &mHandler;
};
