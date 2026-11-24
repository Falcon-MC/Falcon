#include "World/Actor/Mob/Hostile/ElderGuardianActor.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"

FALCON_REGISTER_ACTOR(ElderGuardianActor, ElderGuardianActor::IDENTIFIER);

namespace {
    const uint32_t CURSE_INTERVAL = 1200;
    const float CURSE_RANGE = 50.0f;
    const int32_t FATIGUE_AMPLIFIER = 2;
    const int32_t FATIGUE_DURATION = 6000;
    // the client shows the elder's ghost and plays its curse for this level event
    const int32_t GUARDIAN_CURSE_EVENT = 2006;
}

void ElderGuardianActor::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);

    if (!isAlive() || isDead() || isExpired())
        return;

    // the runtime id spreads the elders of a monument over the minute
    if ((++mTicks + getRuntimeId()) % CURSE_INTERVAL == 0)
        _curse(owner);
}

void ElderGuardianActor::_curse(ServerNetworkHandler &owner) {
    const Vector3f &position = getPosition();
    owner.forEachPlayerNear(position, CURSE_RANGE, [&](ServerPlayer &target) {
        ServerPlayer *player = &target;
        if (!player->isAlive() || !sharesLevelWith(*player))
            return;
        if (!player->isSurvival() && !player->isAdventure())
            return;

        const Vector3f &at = player->getPosition();
        const float dx = at.x - position.x;
        const float dy = at.y - position.y;
        const float dz = at.z - position.z;
        if (dx * dx + dy * dy + dz * dz >= CURSE_RANGE * CURSE_RANGE)
            return;

        // a stronger and longer fatigue is left alone
        const MobEffectInstance *fatigue = player->getEffect(MobEffectId::MiningFatigue);
        if (fatigue != nullptr && fatigue->mAmplifier >= FATIGUE_AMPLIFIER
            && (fatigue->mInfinite || fatigue->mDuration >= CURSE_INTERVAL))
            return;

        owner.addEffect(*player, MobEffectId::MiningFatigue, FATIGUE_AMPLIFIER, FATIGUE_DURATION);
        LevelEventPacket curse;
        curse.mEventId = GUARDIAN_CURSE_EVENT;
        curse.mPosition = at;
        curse.mData = 0;
        owner.sendPacketTo(player->getNetworkIdentifier(), curse);
    });
}
