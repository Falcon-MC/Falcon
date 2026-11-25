#pragma once

#include "Protocol/Types/CameraTypes.h"

#include <string>
#include <vector>

inline const std::vector<CameraPreset> &standardCameraPresets() {
    static const std::vector<CameraPreset> presets = [] {
        std::vector<CameraPreset> list;
        const char *identifiers[] = {
                "minecraft:first_person",
                "minecraft:free",
                "minecraft:third_person",
                "minecraft:third_person_front",
        };
        for (const char *identifier: identifiers) {
            CameraPreset preset;
            preset.mIdentifier = identifier;
            list.push_back(preset);
        }
        return list;
    }();
    return presets;
}

inline int cameraPresetRuntimeId(const std::string &identifier) {
    const std::vector<CameraPreset> &presets = standardCameraPresets();
    for (size_t index = 0; index < presets.size(); ++index) {
        if (presets[index].mIdentifier == identifier)
            return (int) index;
    }
    return -1;
}
