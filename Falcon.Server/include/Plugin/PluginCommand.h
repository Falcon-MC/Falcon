#pragma once

#include "Command/Command.h"
#include "Plugin/LoadedPlugin.h"

#include <falcon/falcon_api.h>

#include <string>
#include <vector>

class PluginCommand : public Command {
public:
    PluginCommand(LoadedPlugin &plugin, const std::string &name, const FalconCommandDescriptor &descriptor);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

private:
    LoadedPlugin &mPlugin;
    FalconCommandHandler mHandler;
    void *mUserData;
    CommandPermission mPermission;
};
