#pragma once

#include "Protocol/Types/StartGameTypes.h"

enum class AdventureSetting {
    WorldImmutable,
    NoPvm,
    NoMvp,
    ShowNameTags,
    AutoJump,
    MayFly,
    NoClip,
    WorldBuilder,
    Flying,
    Muted,
    Mine,
    DoorsAndSwitches,
    OpenContainers,
    AttackPlayers,
    AttackMobs,
    Operator,
    Teleport,
    Build,
    DefaultLevelPermissions,
};

enum class CommandPermission {
    Any,
    GameDirectors,
    Admin,
    Host,
    Owner,
    Internal,
};

