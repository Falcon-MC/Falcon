#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3i.h"

class Actor;
class Level;
class ServerNetworkHandler;

class OpenableBlock {
public:
    static bool isOpen(const BlockState &state);

    static void setOpen(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                        bool open);

    static bool hasManualOverride(Level &level, const Vector3i &position);

    static void setManualOverride(Level &level, const Vector3i &position, bool manual);

    static void forgetLevel(const Level &level);

    // source: who swung it, null for redstone or a wind burst
    static bool toggle(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                       Actor *source = nullptr);

    // block_open or block_close, once per door, gate or trapdoor
    static void postToggled(Level &level, const Vector3i &position, bool open, Actor *source);

    static void openOnPlace(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);
};
