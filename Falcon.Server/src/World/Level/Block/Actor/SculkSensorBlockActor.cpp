#include "World/Level/Block/Actor/SculkSensorBlockActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/SculkSensorBlock.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

FALCON_REGISTER_BLOCK_ACTOR(SculkSensorBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CalibratedSculkSensorBlockActor);

namespace {
    const char *TAG_LISTENER = "VibrationListener";

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

VibrationListener &SculkSensorBlockActor::listener() {
    if (!mListener.has_value())
        mListener.emplace(*this, mPosition, getRange());
    return *mListener;
}

// the listener's compound, its selector at least
Tag SculkSensorBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    if (mListener.has_value()) {
        data.put(TAG_LISTENER, mListener->save());
    } else {
        Tag idle = Tag::ofCompound();
        idle.put("selector", Tag::ofCompound());
        data.put(TAG_LISTENER, idle);
    }
    return data;
}

void SculkSensorBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    const Tag *saved = data.get(TAG_LISTENER);
    if (saved != nullptr && saved->isCompound())
        listener().load(*saved);
}

bool SculkSensorBlockActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
    if (mLevel != nullptr)
        listener().tick(*mLevel);
    return true;
}

// its own breaking or placing is not a vibration, sculk_touch only on the block itself, and only an
// inactive sensor listens
bool SculkSensorBlockActor::shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) {
    const bool here = blockOf(context.mPosition) == mPosition;
    if (here && (&event == &GameEvents::BLOCK_DESTROY || &event == &GameEvents::BLOCK_PLACE))
        return false;
    if (!here && &event == &GameEvents::SCULK_TOUCH)
        return false;

    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    return state != nullptr && SculkSensorBlock::phaseOf(*state) == SculkSensorBlock::PHASE_INACTIVE;
}

// 15 at the sensor, down to 1 at the edge of its range
void SculkSensorBlockActor::onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event,
                                            Actor *source, float distance, uint32_t range, Actor *projectileOwner) {
    (void) position;
    (void) projectileOwner;
    ServerNetworkHandler *owner = level.getOwner();
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (owner == nullptr || state == nullptr || SculkSensorBlock::phaseOf(*state) == SculkSensorBlock::PHASE_ACTIVE)
        return;

    const int32_t power = std::max(1, 15 - (int32_t) std::floor(distance / (float) range * 15.0f));
    SculkSensorBlock::activate(*owner, level, mPosition, source, power, event.getFrequency());
}
