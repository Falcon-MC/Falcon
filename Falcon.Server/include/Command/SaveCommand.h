#pragma once

#include "Command/Command.h"

class ServerNetworkHandler;

class SaveCommand : public Command {
public:
    enum class Mode {
        Save,
        On,
        Off
    };

    SaveCommand(ServerNetworkHandler &handler, Mode mode);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    void setAutoSave(CommandOrigin &sender, bool enabled);

    ServerNetworkHandler &mHandler;
    Mode mMode;
};
