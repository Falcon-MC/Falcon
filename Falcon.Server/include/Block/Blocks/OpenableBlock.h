#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

class Level;
class ServerNetworkHandler;

class OpenableBlock {
public:
    static bool isOpen(const BlockState &state);

    static void setOpen(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                        const BlockState &state, bool open);

    static bool hasManualOverride(Level &level, const Vector3i &position);

    static void setManualOverride(Level &level, const Vector3i &position, bool manual);

    static bool toggle(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                       const BlockState &state);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);
};
