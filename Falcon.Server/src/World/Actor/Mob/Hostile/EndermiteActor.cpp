#include "World/Actor/Mob/Hostile/EndermiteActor.h"

#include "World/Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(EndermiteActor, EndermiteActor::IDENTIFIER);

namespace {
    const char *const TAG_LIFETIME = "Lifetime";
    const char *const NAMEABLE_COMPONENT = "minecraft:nameable";
}

void EndermiteActor::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);

    if (!isAlive() || isDead() || isExpired())
        return;

    if (advanceLifetime())
        despawn();
}

bool EndermiteActor::advanceLifetime() {
    // a named endermite stops ageing but still leaves once past the limit
    const bool named = getComponent(NAMEABLE_COMPONENT) != nullptr && !getNameTag().empty();
    if (!named)
        ++mLifetime;
    return mLifetime >= MAX_LIFETIME;
}

Tag EndermiteActor::saveNbt() const {
    Tag data = HostileActor::saveNbt();
    data.putInt(TAG_LIFETIME, mLifetime);
    return data;
}

void EndermiteActor::loadNbt(const Tag &data) {
    HostileActor::loadNbt(data);
    mLifetime = data.getInt(TAG_LIFETIME, 0);
}
