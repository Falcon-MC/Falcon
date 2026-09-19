#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <string>

class Level;
class ServerNetworkHandler;

class FallingBlockSystem {
public:
    static bool matches(const std::string &identifier);

    static bool breaksOnLava(const std::string &identifier);

    static bool breaksOnGround(const std::string &identifier);

    static bool isConcretePowder(const std::string &identifier);

    static std::string getConcreteFor(const std::string &identifier);

    static std::string getNextAnvilDamage(const std::string &identifier);

    static bool isWater(const std::string &identifier);

    static bool isLava(const std::string &identifier);

    static bool isTransparent(const BlockState &state);

    static bool canFallInto(Level &level, const Vector3i &position);

    static bool isTouchingWater(Level &level, const Vector3i &position);

    static void onNormalUpdate(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    static void onBlockPlaced(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    static void spawnFallingBlock(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    static void setBlockState(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    static void spawnDestroyParticle(ServerNetworkHandler &owner, const Vector3i &position,
                                     const BlockState &state);
};
