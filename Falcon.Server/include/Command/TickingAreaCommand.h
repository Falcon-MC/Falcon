#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;
struct TickingArea;

class TickingAreaCommand : public Command {
public:
    explicit TickingAreaCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    bool _add(CommandOrigin &sender, const std::vector<std::string> &arguments, const Vector3i &origin);

    bool _remove(CommandOrigin &sender, const std::vector<std::string> &arguments, const Vector3i &origin);

    bool _removeAll(CommandOrigin &sender);

    bool _list(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _preload(CommandOrigin &sender, const std::vector<std::string> &arguments, const Vector3i &origin);

    void _sendInUse(CommandOrigin &sender);

    static std::string _describe(const TickingArea &area);

    ServerNetworkHandler &mHandler;
};
