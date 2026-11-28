#pragma once

#include "Protocol/Types/CameraTypes.h"

#include <string>
#include <vector>

struct BehaviorPack;

namespace CameraPresetRegistry {
    void load(const std::vector<BehaviorPack> &packs);

    const std::vector<CameraPreset> &getPresets();

    int runtimeId(const std::string &identifier);
}
