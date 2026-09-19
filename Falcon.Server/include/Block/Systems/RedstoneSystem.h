#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>

class Level;
class ServerNetworkHandler;

enum class RedstoneUpdateType {
    Normal = 0,
    Redstone = 1,
    Scheduled = 2
};

namespace RedstoneFace {
    const int NONE = -1;
    const int DOWN = 0;
    const int UP = 1;
    const int NORTH = 2;
    const int SOUTH = 3;
    const int WEST = 4;
    const int EAST = 5;
    const int COUNT = 6;

    Vector3i offset(int face);

    Vector3i relative(const Vector3i &position, int face);

    int opposite(int face);

    bool isHorizontal(int face);

    int rotateY(int face);

    int rotateYCounterClockwise(int face);

    const char *name(int face);

    int fromName(const std::string &name);
}

class RedstoneSystem {
public:
    static constexpr int MAX_SIGNAL = 15;

    static int64_t packPosition(const Vector3i &position);

    static bool isNormalBlock(const BlockState &state);

    static bool isPowerSource(const BlockState &state);

    static int getWeakPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static int getStrongPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static int getStrongPowerAround(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static int getRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static bool isSidePowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static bool isBlockPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static int isBlockIndirectlyGettingPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static bool isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static void updateAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     int ignoredFace = RedstoneFace::NONE);

    static void updateAllAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        int ignoredFace = RedstoneFace::NONE);

    static void updateAroundNormal(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static void updateComparatorOutputLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                            bool observer);

    static void onUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         RedstoneUpdateType type);

    static void scheduleUpdate(Level &level, const Vector3i &position, int64_t delay);

    static bool isUpdateScheduled(Level &level, const Vector3i &position);

    static void cancelScheduledUpdate(Level &level, const Vector3i &position);

    static void setBlockState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state);

    static void onBlockPlaced(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state);

    static void onBlockBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &previous);

    static void onLeverActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    static void onButtonActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state);

    static void onRepeaterActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state);

    static void onComparatorActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state);

    static int getComparatorOutput(Level &level, const Vector3i &position);

    static void setComparatorOutput(Level &level, const Vector3i &position, int output);

    static void queueRedstoneNotification(Level &level, const Vector3i &position);

    static void tick(ServerNetworkHandler &owner, Level &level);

    static size_t getScheduledCount();

private:
    static void _touchPressurePlate(ServerNetworkHandler &owner, Level &level, const Vector3f &feet,
                                    std::unordered_set<int64_t> &visited);
};
