#include "World/Level/Vibration/VibrationListener.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Level.h"
#include "World/Level/Particle/VibrationSignalParticle.h"
#include "World/Level/Vibration/VibrationProperties.h"

#include <array>
#include <cmath>
#include <limits>

namespace {
    // up first, then down, north, south, west, east
    const std::array<Vector3f, 6> RAY_NUDGES = {Vector3f(0.0f, 1.0f, 0.0f),  Vector3f(0.0f, -1.0f, 0.0f),
                                                Vector3f(0.0f, 0.0f, -1.0f), Vector3f(0.0f, 0.0f, 1.0f),
                                                Vector3f(-1.0f, 0.0f, 0.0f), Vector3f(1.0f, 0.0f, 0.0f)};
    constexpr float RAY_NUDGE = 0.001f;
    constexpr float BELOW_FEET = 0.2f;
    constexpr float SAME_DISTANCE = 0.005f;

    int32_t floorOf(float value) {
        return (int32_t) std::floor(value);
    }

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i(floorOf(position.x), floorOf(position.y), floorOf(position.z));
    }

    Vector3f centerOf(const Vector3i &block) {
        return Vector3f((float) block.x + 0.5f, (float) block.y + 0.5f, (float) block.z + 0.5f);
    }

    bool hasProperty(Level &level, const Vector3i &block, uint8_t property) {
        const BlockState *state = level.peekBlockPtr(block.x, block.y, block.z);
        return state != nullptr && (VibrationProperties::of(*state) & property) != 0;
    }

    Actor *findActor(ServerNetworkHandler &owner, int64_t uniqueId) {
        ServerActor *actor = owner.getActor(uniqueId);
        if (actor != nullptr)
            return actor;
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getUniqueId() == uniqueId)
                return &entry.second;
        }
        return nullptr;
    }

    // a projectile answers for its shooter
    Actor *responsibleActor(Level &level, Actor &source) {
        const ServerActor *actor = dynamic_cast<const ServerActor *>(&source);
        if (actor == nullptr || !actor->isProjectile())
            return &source;
        ServerNetworkHandler *owner = level.getOwner();
        if (owner == nullptr || actor->getOwnerUniqueId() == -1)
            return nullptr;
        return findActor(*owner, actor->getOwnerUniqueId());
    }

    // the cells the segment crosses, its start cell included: a hit on the first occluding one
    bool rayOccluded(Level &level, const Vector3f &from, const Vector3f &to) {
        Vector3i cell = blockOf(from);
        const Vector3i end = blockOf(to);
        const float delta[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
        const float start[3] = {from.x, from.y, from.z};
        int32_t *coordinates[3] = {&cell.x, &cell.y, &cell.z};
        int32_t step[3] = {0, 0, 0};
        float next[3];
        float increment[3];
        for (int axis = 0; axis < 3; ++axis) {
            next[axis] = std::numeric_limits<float>::infinity();
            increment[axis] = std::numeric_limits<float>::infinity();
            if (delta[axis] == 0.0f)
                continue;
            step[axis] = delta[axis] > 0.0f ? 1 : -1;
            const float boundary = (float) (*coordinates[axis] + (step[axis] > 0 ? 1 : 0));
            next[axis] = (boundary - start[axis]) / delta[axis];
            increment[axis] = (float) step[axis] / delta[axis];
        }

        while (true) {
            if (hasProperty(level, cell, VibrationProperties::OCCLUDES))
                return true;
            if (cell == end)
                return false;
            int axis = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
            if (next[axis] > 1.0f)
                return false;
            *coordinates[axis] += step[axis];
            next[axis] += increment[axis];
        }
    }
}

Tag VibrationInfo::save() const {
    Tag data = Tag::ofCompound();
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    data.putFloat("distance", mDistance);
    if (mEvent->getId() != 0)
        data.putInt("vibration", mEvent->getId());
    if (mProjectileOwner.has_value())
        data.putLong("projectile", *mProjectileOwner);
    if (mSource.has_value())
        data.putLong("source", *mSource);
    return data;
}

VibrationInfo VibrationInfo::load(const Tag &data) {
    VibrationInfo info;
    info.mPosition = Vector3i(data.getInt("x"), data.getInt("y"), data.getInt("z"));
    info.mDistance = data.getFloat("distance");
    if (data.contains("vibration"))
        info.mEvent = &GameEvents::fromId(data.getInt("vibration"));
    if (data.contains("projectile"))
        info.mProjectileOwner = data.getLong("projectile");
    if (data.contains("source"))
        info.mSource = data.getLong("source");
    return info;
}

VibrationListener::VibrationListener(VibrationListenerConfig &config, const Vector3i &blockPosition, uint32_t range)
        : mConfig(config), mBlockPosition(blockPosition), mCenter(centerOf(blockPosition)), mRange(range) {}

bool VibrationListener::getPosition(Level &level, Vector3f &position) const {
    (void) level;
    position = mCenter;
    return true;
}

// VibrationListener::handleGameEvent: one signal at a time, then the filters in game order
void VibrationListener::handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) {
    if (mPending.has_value() || !mConfig.canReceive(event))
        return;

    if (event.has(GameEvent::DAMPENED_BY_BLOCK) && context.mBlock != nullptr
        && (VibrationProperties::of(*context.mBlock) & VibrationProperties::DAMPENS) != 0)
        return;

    if (event.has(GameEvent::IGNORED_WHILE_SNEAKING) && context.mSource != nullptr) {
        const Actor *responsible = responsibleActor(level, *context.mSource);
        if (responsible != nullptr && responsible->getFlags().get(ActorFlag::Sneaking))
            return;
    }

    if (event.has(GameEvent::DAMPENED_UNDER_SOURCE) && context.mSource != nullptr) {
        // the standing block: 0.2 below the feet, then the feet block
        const Vector3f &feet = context.mSource->getPosition();
        if (hasProperty(level, blockOf(Vector3f(feet.x, feet.y - BELOW_FEET, feet.z)), VibrationProperties::DAMPENS)
            || hasProperty(level, blockOf(feet), VibrationProperties::DAMPENS))
            return;
    }

    if (!mConfig.shouldListen(level, event, context))
        return;

    // occluded only when all six nudged rays from the source block centre hit an occluding block
    const Vector3f from = centerOf(blockOf(context.mPosition));
    bool reached = false;
    for (const Vector3f &nudge: RAY_NUDGES) {
        const Vector3f start(from.x + nudge.x * RAY_NUDGE, from.y + nudge.y * RAY_NUDGE, from.z + nudge.z * RAY_NUDGE);
        if (!rayOccluded(level, start, mCenter)) {
            reached = true;
            break;
        }
    }
    if (!reached)
        return;

    ServerNetworkHandler *owner = level.getOwner();
    if (owner != nullptr)
        _select(owner->getCurrentTick(), event, context);
}

// the candidates of one tick only, the closest wins, a tie goes to
// the higher frequency
void VibrationListener::_select(int64_t tick, const GameEvent &event, const GameEventContext &context) {
    if (mSelectorTick != -1 && tick != mSelectorTick)
        return;

    const Vector3i position = blockOf(context.mPosition);
    const int64_t dx = (int64_t) mBlockPosition.x - position.x;
    const int64_t dy = (int64_t) mBlockPosition.y - position.y;
    const int64_t dz = (int64_t) mBlockPosition.z - position.z;
    const float distance = (float) (uint64_t) std::sqrt((double) (dx * dx + dy * dy + dz * dz));
    if (mSelectorTick != -1 && mCandidate.has_value()) {
        if (std::fabs(mCandidate->mDistance - distance) < SAME_DISTANCE) {
            if (mCandidate->mEvent->getFrequency() >= event.getFrequency())
                return;
        } else if (distance > mCandidate->mDistance) {
            return;
        }
    }

    mSelectorTick = tick;
    VibrationInfo candidate;
    candidate.mPosition = position;
    candidate.mDistance = distance;
    candidate.mEvent = &event;
    if (context.mSource != nullptr) {
        candidate.mSource = context.mSource->getUniqueId();
        const ServerActor *actor = dynamic_cast<const ServerActor *>(context.mSource);
        if (actor != nullptr && actor->isProjectile() && actor->getOwnerUniqueId() != -1)
            candidate.mProjectileOwner = actor->getOwnerUniqueId();
    }
    mCandidate = candidate;
}

bool VibrationListener::_adjacentChunksTicking(Level &level) const {
    const int32_t chunkX = mBlockPosition.x >> 4;
    const int32_t chunkZ = mBlockPosition.z >> 4;
    for (int32_t x = chunkX - 1; x <= chunkX + 1; ++x) {
        for (int32_t z = chunkZ - 1; z <= chunkZ + 1; ++z) {
            if (!level.isColumnActive(x, z))
                return false;
        }
    }
    return true;
}

// VibrationListener::tick: a candidate of an earlier tick leaves as a signal, travels a tick a block
void VibrationListener::tick(Level &level) {
    ServerNetworkHandler *owner = level.getOwner();
    if (!mPending.has_value() && owner != nullptr && mSelectorTick != -1 && mSelectorTick != owner->getCurrentTick()) {
        std::optional<VibrationInfo> chosen = std::move(mCandidate);
        mSelectorTick = -1;
        mCandidate.reset();
        if (chosen.has_value()) {
            const float travelSeconds = chosen->mDistance / SIGNAL_SPEED;
            mTravelTicks = (int32_t) (SIGNAL_SPEED * travelSeconds);
            mPending = chosen;
            level.addParticle(VibrationSignalParticle(chosen->mPosition, mCenter, travelSeconds));
        }
    }

    if (!mPending.has_value() || --mTravelTicks > 0)
        return;
    if (mConfig.requiresAdjacentChunksToBeTicking() && !_adjacentChunksTicking(level))
        return;

    const VibrationInfo arrived = *mPending;
    mLastEvent = arrived.mEvent;
    Actor *source = owner != nullptr && arrived.mSource.has_value() ? findActor(*owner, *arrived.mSource) : nullptr;
    Actor *projectileOwner = owner != nullptr && arrived.mProjectileOwner.has_value()
                                     ? findActor(*owner, *arrived.mProjectileOwner)
                                     : nullptr;
    mConfig.onSignalReceive(level, arrived.mPosition, *arrived.mEvent, source, arrived.mDistance, mRange,
                            projectileOwner);
    mPending.reset();
}

Tag VibrationListener::save() const {
    Tag data = Tag::ofCompound();
    Tag selector = Tag::ofCompound();
    if (mSelectorTick != -1)
        selector.putInt("tick", (int32_t) mSelectorTick);
    if (mCandidate.has_value())
        selector.put("context", mCandidate->save());
    data.put("selector", selector);
    if (mPending.has_value()) {
        data.putInt("ticks", mTravelTicks);
        data.put("pending", mPending->save());
    }
    if (mLastEvent->getId() != 0)
        data.putInt("event", mLastEvent->getId());
    return data;
}

void VibrationListener::load(const Tag &data) {
    const Tag *selector = data.get("selector");
    if (selector != nullptr && selector->isCompound()) {
        if (selector->contains("tick"))
            mSelectorTick = selector->getInt("tick");
        const Tag *candidate = selector->get("context");
        if (candidate != nullptr && candidate->isCompound())
            mCandidate = VibrationInfo::load(*candidate);
    }
    if (data.contains("ticks"))
        mTravelTicks = data.getInt("ticks");
    const Tag *pending = data.get("pending");
    if (pending != nullptr && pending->isCompound())
        mPending = VibrationInfo::load(*pending);
    if (data.contains("event"))
        mLastEvent = &GameEvents::fromId(data.getInt("event"));
}
