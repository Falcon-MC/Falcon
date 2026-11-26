#pragma once

#include "Server/Commands/Command.h"

class NpcActor;
class ServerNetworkHandler;

class DialogueCommand : public Command {
public:
    explicit DialogueCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    bool _open(CommandOrigin &sender, NpcActor &npc, const std::vector<std::string> &arguments);

    bool _change(CommandOrigin &sender, NpcActor &npc, const std::vector<std::string> &arguments);

    ServerNetworkHandler &mHandler;
};
