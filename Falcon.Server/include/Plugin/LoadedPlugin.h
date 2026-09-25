#pragma once

#include "Plugin/NativeLibrary.h"
#include "Plugin/PluginDescription.h"

#include <falcon/falcon_api.h>

#include <memory>
#include <string>

struct LoadedPlugin {
    PluginDescription mDescription;
    std::string mDirectory;
    std::string mDataFolder;
    std::unique_ptr<NativeLibrary> mLibrary;
    FalconPluginCallbacks mCallbacks{};
    bool mEnabled = false;
};
