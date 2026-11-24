#pragma once

#include "Falcon/Plugin/NativeLibrary.h"
#include "Falcon/Plugin/PluginDescription.h"

#include <falcon/falcon_api.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

struct LoadedPlugin {
    PluginDescription mDescription;
    std::string mDirectory;
    std::string mDataFolder;
    std::vector<std::string> mCommands;
    std::unordered_set<uint64_t> mWindowPlayers;
    std::unique_ptr<NativeLibrary> mLibrary;
    FalconPluginCallbacks mCallbacks{};
    std::shared_ptr<void> mInstance;
    bool mEnabled = false;
};
