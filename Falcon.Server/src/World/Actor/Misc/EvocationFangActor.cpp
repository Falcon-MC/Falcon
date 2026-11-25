#include "World/Actor/Misc/EvocationFangActor.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <string>

FALCON_REGISTER_ACTOR(EvocationFangActor, "minecraft:evocation_fang");

namespace {
    // the fang's box, wider by this on each horizontal side, is where it bites
    const float BITE_REACH = 0.2f;
    const char *const FANG_SOUND = "fang";

    AxisAlignedBB boxAt(const Vector3f &position, float width, float height, float grow) {
        const float half = width * 0.5f + grow;
        return AxisAlignedBB(position.x - half, position.y, position.z - half, position.x + half,
                             position.y + height, position.z + half);
    }
}

void EvocationFangActor::tick(ServerNetworkHandler &owner) {
    if (getLimitedLifeTicks() < 0)
        setLimitedLife(LIFESPAN_TICKS);
    if (getLimitedLifeTicks() == BITE_TICKS)
        _bite(owner);
}

// magic damage from the summoner to the living things it reaches, the summoner and what it summoned spared
void EvocationFangActor::_bite(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const ActorSize size = getSize();
    const AxisAlignedBB reach = boxAt(position, size.mWidth, size.mHeight, BITE_REACH);
    const int64_t summonerId = getOwnerUniqueId();

    Actor *summoner = owner.getActor(summonerId);
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getUniqueId() == summonerId)
            summoner = &entry.second;
    }

    const auto sourceFor = [&](const Actor &victim) {
        ActorDamageSource source =
                summoner != nullptr
                        ? ActorDamageSource::attack("death.attack.indirectMagic", victim.getName(), *summoner,
                                                    summoner->getName(), position)
                        : ActorDamageSource::environment("death.attack.magic", victim.getName());
        source.withoutArmor().withDamager(this);
        return source;
    };

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isAlive() || !player.sharesLevelWith(*this)
            || player.getUniqueId() == summonerId)
            continue;
        const AxisAlignedBB body =
                boxAt(player.getPosition(), MovementPhysics::PLAYER_WIDTH, MovementPhysics::PLAYER_HEIGHT, 0.0f);
        if (!body.intersectsWith(reach))
            continue;
        owner.hurt(player, BITE_DAMAGE, sourceFor(player));
    }

    for (auto &entry: owner.getActors()) {
        auto *mob = dynamic_cast<MobActor *>(entry.second.get());
        if (mob == nullptr || !mob->isAlive() || mob->isInvulnerable() || !mob->sharesLevelWith(*this)
            || mob->getUniqueId() == summonerId || (summonerId != -1 && mob->getOwnerUniqueId() == summonerId))
            continue;
        const ActorSize mobSize = mob->getSize();
        if (!boxAt(mob->getPosition(), mobSize.mWidth, mobSize.mHeight, 0.0f).intersectsWith(reach))
            continue;
        owner.damageActor(*mob, BITE_DAMAGE, sourceFor(*mob));
    }

    owner.broadcastActorEvent(*this, EntityEventType::ArmSwing);
    owner.playLevelSound(level, FANG_SOUND, position);
}
