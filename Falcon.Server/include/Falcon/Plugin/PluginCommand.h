#pragma once

#include "Server/Commands/Command.h"
#include "Falcon/Plugin/LoadedPlugin.h"

#include <falcon/falcon_api.h>

#include <string>
#include <vector>

class PluginCommand : public Command {
public:
    PluginCommand(LoadedPlugin &plugin, const std::string &name, const FalconCommandDescriptor &descriptor);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

    void addOverload(CommandOverloadData overload);

    LoadedPlugin &getPlugin() const {
        return mPlugin;
    }

private:
    LoadedPlugin &mPlugin;
    std::vector<CommandOverloadData> mOverloads;
    FalconCommandHandler mHandler;
    void *mUserData;
    CommandPermission mPermission;
};
