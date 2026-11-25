#pragma once

#include "Protocol/Types/AdventureSettingData.h"
#include "Protocol/Types/CommandEnumData.h"
#include "Protocol/Types/CommandParam.h"

#include <cstdint>
#include <string>
#include <vector>

class CommandParamData {
public:
    std::string mName;
    bool mOptional = false;
    bool mHasEnumData = false;
    CommandEnumData mEnumData;
    bool mHasType = false;
    CommandParamType mType = CommandParamType::RawText;
};

class CommandOverloadData {
public:
    bool mChaining = false;
    std::vector<CommandParamData> mParameters;
};

class ChainedSubCommandValue {
public:
    std::string mFirst;
    std::string mSecond;
};

class ChainedSubCommandData {
public:
    std::string mName;
    std::vector<ChainedSubCommandValue> mValues;
};

class CommandData {
public:
    std::string mName;
    std::string mDescription;
    uint16_t mFlags = 0;
    CommandPermission mPermission = CommandPermission::Any;
    bool mHasAliases = false;
    CommandEnumData mAliases;
    std::vector<ChainedSubCommandData> mSubCommands;
    std::vector<CommandOverloadData> mOverloads;
};
