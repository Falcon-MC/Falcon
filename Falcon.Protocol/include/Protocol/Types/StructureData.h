#pragma once

#include "Protocol/Types/StructureSettings.h"

#include <string>

enum class StructureBlockType {
    Data,
    Save,
    Load,
    Corner,
    Invalid,
    Export,
};

enum class StructureRedstoneSaveMode {
    SavesToMemory,
    SavesToDisk,
};

class StructureEditorData {
public:
    std::string mName;
    std::string mFilteredName;
    std::string mDataField;
    bool mIncludingPlayers = false;
    bool mBoundingBoxVisible = true;
    StructureBlockType mType = StructureBlockType::Data;
    StructureSettings mSettings;
    StructureRedstoneSaveMode mRedstoneSaveMode = StructureRedstoneSaveMode::SavesToDisk;
};
