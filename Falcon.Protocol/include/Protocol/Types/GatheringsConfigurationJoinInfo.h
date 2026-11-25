#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/UUID.h"

#include <string>

class GatheringsConfigurationJoinInfo {
public:
    Uuid mExperienceId;
    std::string mExperienceName;
    bool mHasWorldId = false;
    Uuid mWorldId;
    bool mHasWorldName = false;
    std::string mWorldName;
    std::string mCreatorId;
    bool mHasTargetId = false;
    Uuid mTargetId;
    bool mHasScenarioId = false;
    std::string mScenarioId;
    bool mHasServerId = false;
    std::string mServerId;

    void write(BinaryStream &stream) const;

    void read(ReadOnlyBinaryStream &stream);
};
