#pragma once

#include "Plugin/NativeLibrary.h"
#include "Plugin/PluginDescription.h"

#include <falcon/falcon_api.h>

#include <memory>
#include <string>
#include <vector>

struct LoadedPlugin {
    PluginDescription mDescription;
    std::string mDirectory;
    std::string mDataFolder;
    std::vector<std::string> mCommands;
    std::unique_ptr<NativeLibrary> mLibrary;
    FalconPluginCallbacks mCallbacks{};
    std::shared_ptr<void> mInstance;
    bool mEnabled = false;
};
