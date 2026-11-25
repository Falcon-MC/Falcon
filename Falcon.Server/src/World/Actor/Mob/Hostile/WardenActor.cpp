#include "World/Actor/Mob/Hostile/WardenActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorFlags.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cmath>

FALCON_REGISTER_ACTOR(WardenActor, WardenActor::IDENTIFIER);

namespace {
    // a projectile hit from this far does not make the warden turn on its shooter
    const float RANGED_HIT_DISTANCE_SQUARED = 25.0f;
    // the actor event that moves its tendrils toward a vibration
    const EntityEventType VIBRATION_DETECTED = (EntityEventType) 77;

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    float squaredDistance(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }
}

VibrationListener &WardenActor::_listener() {
    if (mListener == nullptr)
        mListener = std::make_unique<VibrationListener>(mVibrationConfig, *this, VIBRATION_RANGE);
    return *mListener;
}

GameEventListener *WardenActor::getGameEventListener() {
    return isAlive() ? &_listener() : nullptr;
}

void WardenActor::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);
    if (isAlive())
        _listener().tick(owner.getLevelFor(*this));
}

// hit: the attacker gets the warden angry at once, and becomes its target unless it shot from afar
void WardenActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    HostileActor::onDamaged(owner, attacker);
    if (attacker == nullptr || attacker == this)
        return;

    getAngerLevel().increase(owner, *this, *attacker,
                             MobAngerLevel::angryThreshold(*this) + MobAngerLevel::angryBoost(*this), false);
    if (getTarget(owner) != nullptr)
        return;
    const bool ranged = getLastHurtSource().mProjectile
                        && squaredDistance(attacker->getPosition(), getPosition()) >= RANGED_HIT_DISTANCE_SQUARED;
    if (!ranged)
        setTarget(owner, attacker->getRuntimeId());
}

// touched: once a second, the warden suspects where it was touched and grows angry at whoever did
void WardenActor::onPushedBy(ServerNetworkHandler &owner, int64_t pusherId) {
    const int64_t now = owner.getCurrentTick();
    if (now < mNextTouchTick)
        return;
    Actor *pusher = owner.getEntity(pusherId);
    if (pusher == nullptr)
        return;

    mNextTouchTick = now + TOUCH_COOLDOWN;
    setSuspectPosition(blockOf(pusher->getPosition()), now);
    getAngerLevel().increase(owner, *this, *pusher, MobAngerLevel::defaultAnnoyingness(*this), true);
}

bool WardenActor::listensTo(ServerNetworkHandler &owner, const GameEventContext &context) {
    const int64_t now = owner.getCurrentTick();
    if (mLastListenTick.has_value() && now - *mLastListenTick < VIBRATION_COOLDOWN)
        return false;
    if (!isAlive() || getFlags().get(ActorFlag::Digging) || getFlags().get(ActorFlag::Emerging))
        return false;

    const Actor *source = context.mSource;
    if (source != nullptr) {
        if (source == this)
            return false;
        // a living source it could never be angry at is not worth turning to; projectiles and items still are
        const bool living = source->isPlayer() || dynamic_cast<const MobActor *>(source) != nullptr;
        if (living && !getAngerLevel().isValidTarget(owner, *this, *source))
            return false;
    }

    mLastListenTick = now;
    return true;
}

void WardenActor::receiveVibration(ServerNetworkHandler &owner, const Vector3i &position, Actor *source,
                                   Actor *projectileOwner) {
    if (!isAlive())
        return;

    const int64_t now = owner.getCurrentTick();
    owner.broadcastActorEvent(*this, VIBRATION_DETECTED);
    mLastVibration = position;
    mLastVibrationTick = now;

    MobAngerLevel &anger = getAngerLevel();
    Vector3i suspect = position;
    if (projectileOwner != nullptr) {
        if (anger.isValidTarget(owner, *this, *projectileOwner)
            && squaredDistance(projectileOwner->getPosition(), getPosition())
                       < PROJECTILE_OWNER_RANGE * PROJECTILE_OWNER_RANGE) {
            // a second projectile soon after the first gives its owner away
            const bool recent = now - mLastProjectileTick < RECENT_PROJECTILE_TICKS;
            if (recent)
                suspect = blockOf(projectileOwner->getPosition());
            anger.increase(owner, *this, *projectileOwner,
                           recent ? MobAngerLevel::defaultAnnoyingness(*this)
                                  : MobAngerLevel::projectileAnnoyingness(*this),
                           true);
        }
        mLastProjectileTick = now;
    } else if (source != nullptr) {
        anger.increase(owner, *this, *source, MobAngerLevel::defaultAnnoyingness(*this), true);
    }

    // not angry yet: it goes and looks where the vibration came from
    if (anger.getHighestAnger() >= MobAngerLevel::angryThreshold(*this))
        return;
    const Actor *active = anger.getActiveEntity(owner, *this);
    if (projectileOwner != nullptr || active == nullptr || active == source)
        setSuspectPosition(suspect, now);
}

// flaps never reach it; a shriek always does
bool WardenActor::VibrationConfig::canReceive(const GameEvent &event) const {
    if (&event == &GameEvents::FLAP)
        return false;
    if (&event == &GameEvents::SHRIEK)
        return true;
    return event.has(GameEvent::VIBRATION);
}

bool WardenActor::VibrationConfig::shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) {
    (void) event;
    ServerNetworkHandler *owner = level.getOwner();
    return owner != nullptr && mWarden.listensTo(*owner, context);
}

void WardenActor::VibrationConfig::onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event,
                                                    Actor *source, float distance, uint32_t range,
                                                    Actor *projectileOwner) {
    (void) event;
    (void) distance;
    (void) range;
    if (ServerNetworkHandler *owner = level.getOwner())
        mWarden.receiveVibration(*owner, position, source, projectileOwner);
}
