#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Core/Utility/UUID.h"

#include <cstdint>
#include <string>

enum class CommandOriginType : int32_t {
    Player = 0,
    Block = 1,
    MinecartBlock = 2,
    DevConsole = 3,
    Test = 4,
    AutomationPlayer = 5,
    ClientAutomation = 6,
    DedicatedServer = 7,
    Entity = 8,
    Virtual = 9,
    GameArgument = 10,
    EntityServer = 11,
    Precompiled = 12,
    GameDirectorEntityServer = 13,
    Scripting = 14,
    ExecuteContext = 15,
};

class CommandOriginData {
public:
    CommandOriginType mOrigin = CommandOriginType::Player;
    Uuid mUuid;
    std::string mRequestId;
    int64_t mPlayerId = -1;

    void write(BinaryStream &stream) const;

    void read(ReadOnlyBinaryStream &stream);
};
