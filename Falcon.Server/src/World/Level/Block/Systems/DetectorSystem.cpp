#include "World/Level/Block/Systems/DetectorSystem.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {
    const char *HOOK = "minecraft:tripwire_hook";
    const char *WIRE = "minecraft:trip_wire";
    const char *TARGET = "minecraft:target";

    struct TargetPower {
        int32_t mPower = 0;
        int64_t mExpires = 0;
    };

    struct DetectorState {
        // powered wires and the last tick an entity stood on them
        std::unordered_map<int64_t, int64_t> mPoweredWires;
        std::unordered_map<int64_t, Vector3i> mWirePositions;
        std::unordered_map<int64_t, TargetPower> mTargets;
        std::unordered_map<int64_t, Vector3i> mTargetPositions;
        // RedstoneSystem ticks each level once a game tick
        int64_t mTick = 0;
    };

    std::unordered_map<const Level *, DetectorState> gStates;

    DetectorState &stateOf(Level &level) {
        return gStates[&level];
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        if (level.peekBlockPtr(position.x, position.y, position.z) == nullptr)
            return BlockState();
        return level.getBlockState(position.x, position.y, position.z);
    }

    void setPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                    bool powered) {
        Tag states = state.mStates;
        states.putByte("powered_bit", powered ? 1 : 0);
        const BlockState updated(state.mName, states);
        level.setBlock(position, updated, false);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, updated);
    }

    // a hook is powered while a wire on its line is
    void refreshHook(ServerNetworkHandler &owner, Level &level, const Vector3i &hook) {
        const BlockState state = stateAt(level, hook);
        if (!DetectorSystem::isHook(state))
            return;
        const int facing = DetectorSystem::hookFacing(state);
        bool powered = false;
        Vector3i cursor = hook;
        for (int32_t step = 0; step < DetectorSystem::MAX_WIRE_LENGTH; ++step) {
            cursor = RedstoneFace::relative(cursor, facing);
            const BlockState wire = stateAt(level, cursor);
            if (!DetectorSystem::isWire(wire))
                break;
            if (wire.mStates.getBool("powered_bit", false))
                powered = true;
        }
        if (state.mStates.getBool("powered_bit", false) == powered)
            return;
        setPowered(owner, level, hook, state, powered);
        RedstoneSystem::updateAroundRedstone(owner, level, hook);
        RedstoneSystem::updateAroundRedstone(owner, level,
                                             RedstoneFace::relative(hook, RedstoneFace::opposite(facing)));
    }

    // the hooks looking down the wire's line from either side
    void refreshHooksOf(ServerNetworkHandler &owner, Level &level, const Vector3i &wire) {
        for (const int face: {RedstoneFace::NORTH, RedstoneFace::SOUTH, RedstoneFace::WEST, RedstoneFace::EAST}) {
            Vector3i cursor = wire;
            for (int32_t step = 0; step < DetectorSystem::MAX_WIRE_LENGTH; ++step) {
                cursor = RedstoneFace::relative(cursor, face);
                const BlockState state = stateAt(level, cursor);
                if (DetectorSystem::isHook(state)) {
                    if (DetectorSystem::hookFacing(state) == RedstoneFace::opposite(face))
                        refreshHook(owner, level, cursor);
                    break;
                }
                if (!DetectorSystem::isWire(state))
                    break;
            }
        }
    }

    std::vector<Actor *> actorsOn(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        std::vector<Actor *> found;
        const auto inside = [&](const Vector3f &feet) {
            return (int32_t) std::floor(feet.x) == position.x && (int32_t) std::floor(feet.y) == position.y
                   && (int32_t) std::floor(feet.z) == position.z;
        };
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (player.isSpawned() && !player.isDead() && player.isIn(level) && inside(player.getPosition()))
                found.push_back(&player);
        }
        for (auto &entry: owner.getActors()) {
            ServerActor *actor = entry.second.get();
            if (actor != nullptr && !actor->isDead() && actor->isIn(level) && inside(actor->getPosition()))
                found.push_back(actor);
        }
        return found;
    }
}

bool DetectorSystem::isHook(const BlockState &state) {
    return state.mName == HOOK;
}

bool DetectorSystem::isWire(const BlockState &state) {
    return state.mName == WIRE;
}

bool DetectorSystem::isTarget(const BlockState &state) {
    return state.mName == TARGET;
}

int DetectorSystem::hookFacing(const BlockState &state) {
    static const int FACES[] = {RedstoneFace::SOUTH, RedstoneFace::WEST, RedstoneFace::NORTH, RedstoneFace::EAST};
    return FACES[std::clamp(state.mStates.getInt("direction", 0), 0, 3)];
}

int DetectorSystem::weakPower(Level &level, const Vector3i &position, const BlockState &state, int face) {
    (void) face;
    if (isHook(state))
        return state.mStates.getBool("powered_bit", false) ? RedstoneSystem::MAX_SIGNAL : 0;
    if (isTarget(state)) {
        const auto found = stateOf(level).mTargets.find(RedstoneSystem::packPosition(position));
        return found == stateOf(level).mTargets.end() ? 0 : found->second.mPower;
    }
    return -1;
}

int DetectorSystem::strongPower(Level &level, const Vector3i &position, const BlockState &state, int face) {
    (void) level;
    (void) position;
    if (isHook(state)) {
        if (!state.mStates.getBool("powered_bit", false))
            return 0;
        return face == RedstoneFace::opposite(hookFacing(state)) ? RedstoneSystem::MAX_SIGNAL : 0;
    }
    if (isTarget(state))
        return 0;
    return -1;
}

void DetectorSystem::tick(ServerNetworkHandler &owner, Level &level) {
    DetectorState &detectors = stateOf(level);
    const int64_t now = ++detectors.mTick;

    const auto visit = [&](Actor &actor) {
        const Vector3f &feet = actor.getPosition();
        const Vector3i cell((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y), (int32_t) std::floor(feet.z));
        const BlockState state = stateAt(level, cell);
        if (!isWire(state) || state.mStates.getBool("disarmed_bit", false))
            return;
        const int64_t key = RedstoneSystem::packPosition(cell);
        const bool known = detectors.mPoweredWires.count(key) != 0;
        detectors.mPoweredWires[key] = now;
        detectors.mWirePositions[key] = cell;
        if (known && state.mStates.getBool("powered_bit", false))
            return;
        setPowered(owner, level, cell, state, true);
        refreshHooksOf(owner, level, cell);
        std::vector<Actor *> sources = actorsOn(owner, level, cell);
        owner.getScriptEngine().onTripWireTrip(level, cell, true, sources);
    };
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && !player.isDead() && player.isIn(level))
            visit(player);
    }
    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor != nullptr && !actor->isDead() && actor->isIn(level))
            visit(*actor);
    }

    std::vector<int64_t> released;
    for (const auto &entry: detectors.mPoweredWires) {
        if (now - entry.second >= WIRE_RECHECK_TICKS)
            released.push_back(entry.first);
    }
    for (const int64_t key: released) {
        const Vector3i cell = detectors.mWirePositions[key];
        detectors.mPoweredWires.erase(key);
        detectors.mWirePositions.erase(key);
        const BlockState state = stateAt(level, cell);
        if (!isWire(state))
            continue;
        setPowered(owner, level, cell, state, false);
        refreshHooksOf(owner, level, cell);
        owner.getScriptEngine().onTripWireTrip(level, cell, false, std::vector<Actor *>());
    }

    std::vector<int64_t> expired;
    for (const auto &entry: detectors.mTargets) {
        if (now >= entry.second.mExpires)
            expired.push_back(entry.first);
    }
    for (const int64_t key: expired) {
        const Vector3i position = detectors.mTargetPositions[key];
        detectors.mTargets.erase(key);
        detectors.mTargetPositions.erase(key);
        RedstoneSystem::updateAroundRedstone(owner, level, position);
    }
}

int DetectorSystem::targetPowerFor(const Vector3i &position, const Vector3f &at, const Vector3f &previous) {
    const float local[3] = {at.x - (float) position.x, at.y - (float) position.y, at.z - (float) position.z};
    const float travel[3] = {at.x - previous.x, at.y - previous.y, at.z - previous.z};
    // the struck face is across the axis the projectile moved most along
    int axis = 0;
    for (int candidate = 1; candidate < 3; ++candidate) {
        if (std::fabs(travel[candidate]) > std::fabs(travel[axis]))
            axis = candidate;
    }
    float distance = 0.0f;
    for (int other = 0; other < 3; ++other) {
        if (other != axis)
            distance = std::max(distance, std::fabs(std::clamp(local[other], 0.0f, 1.0f) - 0.5f));
    }
    const float precision = std::clamp((0.5f - distance) / 0.5f, 0.0f, 1.0f);
    return std::max(1, (int) std::ceil(15.0f * precision));
}

void DetectorSystem::onTargetHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const Vector3f &at, const Vector3f &previous, Actor &projectile, bool arrowLike) {
    DetectorState &detectors = stateOf(level);
    const int64_t key = RedstoneSystem::packPosition(position);
    const auto known = detectors.mTargets.find(key);
    const int32_t previousPower = known == detectors.mTargets.end() ? 0 : known->second.mPower;
    const int32_t power = targetPowerFor(position, at, previous);
    detectors.mTargets[key] = TargetPower{power,
                                          detectors.mTick + (arrowLike ? TARGET_ARROW_TICKS : TARGET_OTHER_TICKS)};
    detectors.mTargetPositions[key] = position;
    RedstoneSystem::updateAroundRedstone(owner, level, position);
    owner.getScriptEngine().onTargetBlockHit(level, position, at, previousPower, power, &projectile);
}
