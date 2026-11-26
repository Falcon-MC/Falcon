#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Level/BlockUpdateType.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>

class Actor;
class CircuitSystem;
class Level;
class LevelChunk;
class ServerNetworkHandler;

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

// the blocks' side of the game's circuit: every redstone block holds a component of the level's circuit,
// which evaluates every second tick and hands each changed component its new strength
class RedstoneSystem {
public:
    static constexpr int MAX_SIGNAL = 15;

    static int64_t packPosition(const Vector3i &position);

    static Vector3i unpackPosition(int64_t packed);

    static bool isNormalBlock(const BlockState &state);

    static bool canConnectWireTo(Level &level, const Vector3i &position, int side);

    static bool isPowerSource(const BlockState &state);

    static CircuitSystem &circuitOf(Level &level);

    // the strength of the component at `position`, 0 without one
    static int getStrength(Level &level, const Vector3i &position);

    // the signal the source at `position` sends towards `face`; strong only reaches through a block it is on
    static int getWeakPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static int getStrongPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    static int getRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face);

    // the strength the consumer at `position` receives; elsewhere, the strongest power its neighbours give it
    static int isBlockIndirectlyGettingPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static bool isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    // the source at `position` now gives another signal: its component takes it
    static void updateAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    // every block change: components follow the block, and the sources around search again
    static void onBlockChanged(Level &level, const Vector3i &position, const BlockState &previous,
                               const BlockState &current);

    // an observer a piston has just set down lights at once
    static void onObserverMoved(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, BlockUpdateType type);

    static void onChunkLoaded(ServerNetworkHandler &owner, Level &level, const LevelChunk &chunk);

    static void onChunkUnloaded(Level &level, int32_t chunkX, int32_t chunkZ);

    static void forgetLevel(Level &level);

    static void moveLevelState(Level &from, Level &to);

    static void onLeverActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    // source: who pressed it, null for a wind burst or a script
    static void onButtonActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state, Actor *source = nullptr);

    static void onRepeaterActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state);

    static void onComparatorActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state);

    static int getComparatorOutput(Level &level, const Vector3i &position);

    // a block's comparator signal or emitted power changed without a state change
    static void queueRedstoneNotification(Level &level, const Vector3i &position);

    // before the level's scheduled ticks: the circuit evaluates every second tick
    static void tickCircuit(ServerNetworkHandler &owner, Level &level);

    // after them: components hear their strength, actors press plates, the graph takes the changes
    static void tick(ServerNetworkHandler &owner, Level &level);

private:
    // the actor standing on it is the push event's source
    static void _touchPressurePlate(ServerNetworkHandler &owner, Level &level, Actor &actor,
                                    std::unordered_set<int64_t> &visited);
};
