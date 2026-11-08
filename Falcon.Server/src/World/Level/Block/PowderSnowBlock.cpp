#include "World/Level/Block/PowderSnowBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Actor/Actor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <cmath>

FALCON_REGISTER_BLOCK(PowderSnowBlock, 185);

namespace {
    const Vector3f STUCK_MULTIPLIER(0.9f, 1.5f, 0.9f);
    // LevelSoundEvent 27
    const char *FIZZ_SOUND = "fizz";
    const int32_t LEVEL_EVENT_FIZZ = 2029;
    const int32_t LEVEL_EVENT_DESTROY_NO_SOUND = 2021;

    // sub_14A23B050: melts with the destroy particles and no break sound
    void melt(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        level.setBlock(position, BlockState("minecraft:air"), true);
        owner.broadcastLevelEvent(level, LEVEL_EVENT_DESTROY_NO_SOUND,
                                  Vector3f((float) position.x, (float) position.y, (float) position.z),
                                  (int32_t) BlockStateHasher::hash(state.mName, state.mStates));
    }

    // sub_14CE412A0: every powder snow block the burning actor stands in
    void meltAround(ServerNetworkHandler &owner, Level &level, const Vector3f &feet, const ActorSize &size) {
        const int32_t minX = (int32_t) std::floor(feet.x - size.mWidth * 0.5f);
        const int32_t maxX = (int32_t) std::floor(feet.x + size.mWidth * 0.5f);
        const int32_t minY = (int32_t) std::floor(feet.y);
        const int32_t maxY = (int32_t) std::floor(feet.y + size.mHeight);
        const int32_t minZ = (int32_t) std::floor(feet.z - size.mWidth * 0.5f);
        const int32_t maxZ = (int32_t) std::floor(feet.z + size.mWidth * 0.5f);
        for (int32_t x = minX; x <= maxX; ++x) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    const BlockState *state = level.peekBlockPtr(x, y, z);
                    const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromState(*state);
                    if (block != nullptr && block->freezesActorsInside())
                        melt(owner, level, Vector3i(x, y, z), BlockState(*state));
                }
            }
        }
    }
}

bool PowderSnowBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:powder_snow";
}

void PowderSnowBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                    const BlockState &state) const {
    (void) position;
    (void) state;
    actor.makeStuckInBlock(STUCK_MULTIPLIER);
    actor.markInFreezingBlock(owner.getCurrentTick());
    if (!actor.isOnFire())
        return;
    const auto *player = dynamic_cast<const ServerPlayer *>(&actor);
    if (player != nullptr && player->getGameType() == (int32_t) GameType::Spectator)
        return;

    // sub_143611E40, sub_143531350: put out with a fizz sized to the box
    auto *serverActor = dynamic_cast<ServerActor *>(&actor);
    const ActorSize size = serverActor != nullptr ? serverActor->getSize() : ActorSize{0.6f, 1.8f};
    const Vector3f feet = actor.getPosition();
    Level &level = owner.getLevelFor(actor);
    actor.extinguish();
    // players resync their flags on their own tick, mobs do not
    if (serverActor != nullptr)
        owner.syncActorFlags(*serverActor);
    owner.playLevelSound(level, FIZZ_SOUND, feet);
    const int32_t fizzData = (int32_t) std::ceil(size.mWidth) + ((int32_t) std::ceil(size.mHeight) << 8);
    owner.broadcastLevelEvent(level, LEVEL_EVENT_FIZZ,
                              Vector3f(feet.x - size.mWidth * 0.5f, feet.y, feet.z - size.mWidth * 0.5f), fizzData);

    // mob griefing gamerule, sub_140B9C800 319: players always melt it
    if (level.getGameRules().getBool("mobgriefing") || player != nullptr)
        meltAround(owner, level, feet, size);
}

bool PowderSnowBlock::getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const {
    (void) actor;
    multiplier = STUCK_MULTIPLIER;
    return true;
}
