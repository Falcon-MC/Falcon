#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Particle/GenericLevelEventParticle.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cmath>

FALCON_REGISTER_BLOCK_ACTOR(SculkCatalystBlockActor);

namespace {
    const char *const STATE_BLOOM = "bloom";
    const int32_t EVENT_BLOOM = 2036;
}

Tag SculkCatalystBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    mSpreader.save(data);
    return data;
}

void SculkCatalystBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    mSpreader.load(data);
}

bool SculkCatalystBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel != nullptr)
        mSpreader.updateCursors(&owner, *mLevel, mPosition, mRandom, true);
    return true;
}

bool SculkCatalystBlockActor::getPosition(Level &level, Vector3f &position) const {
    (void) level;
    position = Vector3f((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f);
    return true;
}

// a death near: the experience it would have dropped becomes charge at its block, and the catalyst blooms
void SculkCatalystBlockActor::handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) {
    if (&event != &GameEvents::ENTITY_DIE)
        return;
    Actor *dead = context.mSource;
    ServerNetworkHandler *owner = level.getOwner();
    if (dead == nullptr || owner == nullptr || dead->isHandledBySculkCatalyst())
        return;

    const int32_t experience = dead->takeDeathExperience(*owner);
    if (experience > 0) {
        const Vector3f &feet = dead->getPosition();
        mSpreader.addCursors(Vector3i((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y),
                                      (int32_t) std::floor(feet.z)),
                             experience);
    }
    bloom(level);
    dead->setHandledBySculkCatalyst();
}

void SculkCatalystBlockActor::bloom(Level &level) {
    const BlockState state = level.getBlockState(mPosition.x, mPosition.y, mPosition.z);
    Tag states = state.mStates;
    states.putByte(STATE_BLOOM, 1);
    level.setBlock(mPosition, BlockState(state.mName, states), true);
    level.scheduleUpdate(mPosition, BLOOM_TICKS);

    Tag data = Tag::ofCompound();
    data.putInt("originX", mPosition.x);
    data.putInt("originY", mPosition.y);
    data.putInt("originZ", mPosition.z);
    level.addParticle(GenericLevelEventParticle(
            EVENT_BLOOM, Vector3f((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f),
            std::move(data)));
}
