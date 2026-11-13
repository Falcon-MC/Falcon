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
#include <initializer_list>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    const char *HOOK = "minecraft:tripwire_hook";
    const char *WIRE = "minecraft:trip_wire";
    const char *TARGET = "minecraft:target";

    struct TargetPower {
        int32_t mPower = 0;
        int64_t mExpires = 0;
    };

    // the game keeps a target's power in its redstone producer, not in the block: lost with the chunk
    struct DetectorState {
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

    void setBits(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                 std::initializer_list<std::pair<const char *, bool>> bits) {
        Tag states = state.mStates;
        bool changed = false;
        for (const auto &bit: bits) {
            if (states.getBool(bit.first, false) == bit.second)
                continue;
            states.putByte(bit.first, bit.second ? 1 : 0);
            changed = true;
        }
        if (!changed)
            return;
        const BlockState updated(state.mName, states);
        level.setBlock(position, updated, false);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, updated);
    }

    void notifyHook(ServerNetworkHandler &owner, Level &level, const Vector3i &hook, int facing) {
        RedstoneSystem::updateAroundRedstone(owner, level, hook);
        RedstoneSystem::updateAroundRedstone(owner, level,
                                             RedstoneFace::relative(hook, RedstoneFace::opposite(facing)));
    }

    // attached only to a facing hook over unbroken wire,
    // powered only while attached and a wire that isn't disarmed is
    void refreshHook(ServerNetworkHandler &owner, Level &level, const Vector3i &hook) {
        const BlockState state = stateAt(level, hook);
        if (!DetectorSystem::isHook(state))
            return;
        const int facing = DetectorSystem::hookFacing(state);
        bool unbroken = true;
        bool tripped = false;
        int32_t other = 0;
        std::vector<Vector3i> wires;
        for (int32_t step = 1; step <= DetectorSystem::MAX_HOOK_REACH; ++step) {
            const Vector3i at(hook.x + RedstoneFace::offset(facing).x * step, hook.y,
                              hook.z + RedstoneFace::offset(facing).z * step);
            const BlockState found = stateAt(level, at);
            if (DetectorSystem::isHook(found)) {
                if (DetectorSystem::hookFacing(found) == RedstoneFace::opposite(facing))
                    other = step;
                break;
            }
            if (!DetectorSystem::isWire(found)) {
                unbroken = false;
                continue;
            }
            wires.push_back(at);
            if (found.mStates.getBool("powered_bit", false) && !found.mStates.getBool("disarmed_bit", false))
                tripped = true;
        }
        const bool attached = unbroken && other > 1;
        const bool powered = attached && tripped;
        const bool wasAttached = state.mStates.getBool("attached_bit", false);
        const bool wasPowered = state.mStates.getBool("powered_bit", false);
        setBits(owner, level, hook, state, {{"attached_bit", attached}, {"powered_bit", powered}});
        if (wasPowered != powered || wasAttached != attached)
            notifyHook(owner, level, hook, facing);
        if (other > 0) {
            const Vector3i farHook(hook.x + RedstoneFace::offset(facing).x * other, hook.y,
                               hook.z + RedstoneFace::offset(facing).z * other);
            const BlockState farState = stateAt(level, farHook);
            const bool farChanged = farState.mStates.getBool("attached_bit", false) != attached
                                    || farState.mStates.getBool("powered_bit", false) != powered;
            setBits(owner, level, farHook, farState, {{"attached_bit", attached}, {"powered_bit", powered}});
            if (farChanged)
                notifyHook(owner, level, farHook, RedstoneFace::opposite(facing));
        }
        if (other > 1 && attached != wasAttached) {
            for (const Vector3i &wire: wires)
                setBits(owner, level, wire, stateAt(level, wire), {{"attached_bit", attached}});
        }
    }

    // only south and west; calculateState sets both ends
    void refreshHooksOf(ServerNetworkHandler &owner, Level &level, const Vector3i &wire) {
        for (const int face: {RedstoneFace::SOUTH, RedstoneFace::WEST}) {
            Vector3i cursor = wire;
            for (int32_t step = 0; step < DetectorSystem::MAX_HOOK_REACH; ++step) {
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
        // the producer's strong face is the support behind the hook
        return face == hookFacing(state) ? RedstoneSystem::MAX_SIGNAL : 0;
    }
    if (isTarget(state))
        return 0;
    return -1;
}

namespace {
    // the after event only on a change, a recheck while occupied
    void checkPressed(ServerNetworkHandler &owner, Level &level, const Vector3i &cell, const BlockState &state) {
        std::vector<Actor *> sources = actorsOn(owner, level, cell);
        const bool pressed = !sources.empty();
        if (state.mStates.getBool("powered_bit", false) != pressed) {
            setBits(owner, level, cell, state, {{"powered_bit", pressed}});
            refreshHooksOf(owner, level, cell);
            owner.getScriptEngine().onTripWireTrip(level, cell, pressed, sources);
        }
        if (pressed)
            level.scheduleUpdate(cell, DetectorSystem::WIRE_RECHECK_TICKS);
    }
}

void DetectorSystem::tick(ServerNetworkHandler &owner, Level &level) {
    DetectorState &detectors = stateOf(level);
    const int64_t now = ++detectors.mTick;

    // an unpowered wire only, disarmed or not
    const auto visit = [&](Actor &actor) {
        const Vector3f &feet = actor.getPosition();
        const Vector3i cell((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y), (int32_t) std::floor(feet.z));
        const BlockState state = stateAt(level, cell);
        if (isWire(state) && !state.mStates.getBool("powered_bit", false))
            checkPressed(owner, level, cell, state);
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

void DetectorSystem::onWireScheduled(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) {
    if (isWire(state) && state.mStates.getBool("powered_bit", false))
        checkPressed(owner, level, position, state);
}

// TargetBlock: the struck face is the axis farthest from the centre; power falls with the
// distance from the face centre over sqrt(0.5)
int DetectorSystem::targetPowerFor(const Vector3i &position, const Vector3f &hit) {
    const float epsilon = 1.1920929e-7f;
    const float dx = hit.x - ((float) position.x + 0.5f);
    const float dy = hit.y - ((float) position.y + 0.5f);
    const float dz = hit.z - ((float) position.z + 0.5f);
    const float largest = std::max(std::max(std::fabs(dx), std::fabs(dy)), std::fabs(dz));
    const bool xFace = std::fabs(std::fabs(dx) - largest) <= epsilon;
    const bool yFace = std::fabs(std::fabs(dy) - largest) <= epsilon;
    const bool zFace = std::fabs(std::fabs(dz) - largest) <= epsilon;
    // the game's own tie order between faces
    float first = xFace ? dy : dx;
    if (yFace)
        first = xFace ? 0.0f : dx;
    float second = (xFace || yFace) ? 0.0f : dy;
    const int kept = (xFace ? 0 : 1) + (yFace ? 0 : 1);
    if (kept <= 1 && !zFace) {
        if (kept == 1)
            second = dz;
        else
            first = dz;
    }
    const float reach = 0.70710677f;
    const int power = (int) ((reach - std::sqrt(second * second + first * first)) * 16.0f / reach);
    return std::min(15, power >= 2 ? power : 1);
}

Vector3f DetectorSystem::targetHitPoint(const Vector3i &position, const Vector3f &at, const Vector3f &previous) {
    const float from[3] = {previous.x, previous.y, previous.z};
    const float delta[3] = {at.x - previous.x, at.y - previous.y, at.z - previous.z};
    const float low[3] = {(float) position.x, (float) position.y, (float) position.z};
    float enter = 0.0f;
    float leave = 1.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(delta[axis]) < 1e-7f) {
            if (from[axis] < low[axis] || from[axis] > low[axis] + 1.0f)
                return at;
            continue;
        }
        float closest = (low[axis] - from[axis]) / delta[axis];
        float farthest = (low[axis] + 1.0f - from[axis]) / delta[axis];
        if (closest > farthest)
            std::swap(closest, farthest);
        enter = std::max(enter, closest);
        leave = std::min(leave, farthest);
        if (enter > leave)
            return at;
    }
    return Vector3f(from[0] + delta[0] * enter, from[1] + delta[1] * enter, from[2] + delta[2] * enter);
}

void DetectorSystem::onTargetHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const Vector3f &at, const Vector3f &previous, Actor &projectile, bool arrowLike) {
    DetectorState &detectors = stateOf(level);
    const int64_t key = RedstoneSystem::packPosition(position);
    const auto known = detectors.mTargets.find(key);
    const int32_t previousPower = known == detectors.mTargets.end() ? 0 : known->second.mPower;
    const Vector3f hit = targetHitPoint(position, at, previous);
    const int32_t power = targetPowerFor(position, hit);
    detectors.mTargets[key] = TargetPower{power,
                                          detectors.mTick + (arrowLike ? TARGET_ARROW_TICKS : TARGET_OTHER_TICKS)};
    detectors.mTargetPositions[key] = position;
    RedstoneSystem::updateAroundRedstone(owner, level, position);
    owner.getScriptEngine().onTargetBlockHit(level, position, hit, previousPower, power, &projectile);
}
