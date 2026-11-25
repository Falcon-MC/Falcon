#include "World/Level/Block/Actor/SkullBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"

#include <cmath>
#include <memory>

namespace {
    const char *TAG_SKULL_TYPE = "SkullType";
    const char *TAG_ROTATION = "Rotation";
    const char *TAG_DOING_ANIMATION = "DoingAnimation";
    const char *TAG_MOUTH_TICK_COUNT = "MouthTickCount";
    // older saves: a byte of 22.5 degree steps, and the dragon head's mouth flag
    const char *TAG_LEGACY_ROTATION = "Rot";
    const char *TAG_LEGACY_MOUTH_MOVING = "MouthMoving";

    constexpr float ROTATION_STEP = 22.5f;
}

void SkullBlockActor::_putFields(Tag &data) const {
    data.putByte(TAG_SKULL_TYPE, (int8_t) mSkullType);
    data.putFloat(TAG_ROTATION, mRotation);
    data.putByte(TAG_DOING_ANIMATION, mDoingAnimation ? 1 : 0);
    data.putInt(TAG_MOUTH_TICK_COUNT, mMouthTickCount);
}

Tag SkullBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag SkullBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void SkullBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    const Tag *legacyRotation = data.get(TAG_LEGACY_ROTATION);
    if (legacyRotation != nullptr && legacyRotation->getType() == Tag::Type::Byte)
        mRotation = (float) (uint8_t) data.getByte(TAG_LEGACY_ROTATION, 0) * ROTATION_STEP;
    else
        mRotation = data.getFloat(TAG_ROTATION, 0.0f);
    mSkullType = (uint8_t) data.getByte(TAG_SKULL_TYPE, 0);
    mDoingAnimation = data.getByte(TAG_LEGACY_MOUTH_MOVING, 0) != 0 || data.getByte(TAG_DOING_ANIMATION, 0) != 0;
    mMouthTickCount = data.getInt(TAG_MOUTH_TICK_COUNT, 0);
}

bool SkullBlockActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
    if (mDoingAnimation)
        ++mMouthTickCount;
    return true;
}

bool SkullBlockActor::setDoingAnimation(bool doing) {
    if (mDoingAnimation == doing)
        return false;
    mDoingAnimation = doing;
    return true;
}

float SkullBlockActor::floorRotation(float yaw) {
    return std::floor(yaw * 16.0f / 360.0f + 0.5f) * ROTATION_STEP;
}

void SkullBlockActor::place(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state, float rotation) {
    BlockActorStore &blockActors = level.getBlockActors();
    SkullBlockActor *skull = blockActors.find<SkullBlockActor>(position);
    if (skull == nullptr) {
        std::unique_ptr<SkullBlockActor> created(new SkullBlockActor());
        created->setPosition(position);
        created->setState(state);
        skull = created.get();
        blockActors.insert(std::move(created));
    }
    skull->setRotation(rotation);
    BlockActionHandler::broadcastBlockActorData(owner, level, *skull);
}
