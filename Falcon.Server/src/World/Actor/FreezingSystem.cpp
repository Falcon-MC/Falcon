#include "World/Actor/FreezingSystem.h"

#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

bool FreezingSystem::isSuppressed(Level &level, const Actor &actor) {
    bool suppressed = FreezingMobEffect::isProtected(actor) || FreezingMobEffect::isImmune(actor);
    const auto *serverActor = dynamic_cast<const ServerActor *>(&actor);
    const ActorSize size = serverActor != nullptr ? serverActor->getSize() : ActorSize{0.6f, 1.8f};
    const Vector3f position = actor.getPosition();
    for (int x = (int) std::floor(position.x - size.mWidth * 0.5f);
         !suppressed && x <= (int) std::floor(position.x + size.mWidth * 0.5f - 0.001f); ++x) {
        for (int z = (int) std::floor(position.z - size.mWidth * 0.5f);
             !suppressed && z <= (int) std::floor(position.z + size.mWidth * 0.5f - 0.001f); ++z) {
            for (int y = (int) std::floor(position.y);
                 !suppressed && y <= (int) std::floor(position.y + size.mHeight - 0.001f); ++y) {
                for (int layer = 0; layer < 2; ++layer) {
                    const BlockState *state = level.peekBlockPtr(x, y, z, layer);
                    const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
                    if (block != nullptr && block->suppressesFreezing()) {
                        suppressed = true;
                        break;
                    }
                }
            }
        }
    }
    return suppressed;
}

void FreezingSystem::tick(ServerNetworkHandler &owner, Actor &actor) {
    if (!actor.isAlive() || !actor.hasEffect(MobEffectId::Freezing))
        return;
    Level &level = owner.getLevelFor(actor);
    const float damage = actor.getEffects().advanceFreezing(isSuppressed(level, actor));
    if (damage <= 0.0f || !level.getGameRules().getBool("freezedamage"))
        return;
    const ActorDamageSource source = ActorDamageSource::environment("death.attack.freeze", actor.getName()).withoutArmor();
    if (auto *player = dynamic_cast<ServerPlayer *>(&actor))
        owner.hurt(*player, damage, source);
    else if (auto *mob = dynamic_cast<ServerActor *>(&actor))
        owner.damageActor(*mob, damage, source);
}
