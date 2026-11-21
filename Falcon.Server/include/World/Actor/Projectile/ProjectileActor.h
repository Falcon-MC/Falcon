#pragma once

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/SizedActor.h"

class ProjectileActor : public SizedActor {
public:
    static constexpr float DEFAULT_GRAVITY = 0.05f;

    ProjectileActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size)
            : SizedActor(runtimeId, identifier, size, true) {}

    virtual float getGravity() const {
        return DEFAULT_GRAVITY;
    }

    virtual float getInertia() const {
        return 1.0f;
    }

    // never hits its shooter, however long it flies through its box
    virtual bool passesThroughOwner() const {
        return false;
    }

    // the motion of this tick in place of gravity and inertia; false to fly as any projectile
    virtual bool steer(ServerNetworkHandler &owner, Vector3f &motion) {
        (void) owner;
        (void) motion;
        return false;
    }

    // the tick's move is done
    virtual void afterMove(ServerNetworkHandler &owner) {
        (void) owner;
    }

    // a melee hit: destroy_on_hurt removes it, reflect_on_hurt sends it back where the attacker looks;
    // false when the hit does nothing to it
    bool onMeleeHit(ServerNetworkHandler &owner, Actor &attacker);

    // the projectile flies back along that direction and now belongs to the attacker
    virtual void reflect(Actor &attacker, const Vector3f &direction);

protected:
    // the class physics stand in for a missing definition
    ProjectileComponent _undefinedProjectileComponent() const override {
        ProjectileComponent component;
        component.mGravity = getGravity();
        component.mInertia = getInertia();
        return component;
    }
};

class ThrownProjectileActor : public ProjectileActor {
public:
    using ProjectileActor::ProjectileActor;

    float getGravity() const override {
        return 0.03f;
    }

    float getInertia() const override {
        return 0.99f;
    }

    virtual bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
        (void) owner;
        (void) hitPosition;
        (void) hitPlayer;
        return false;
    }

    virtual bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
        (void) owner;
        (void) hitPosition;
        (void) hitActor;
        return false;
    }

    // a hit that fires a definition event leaves the projectile in place a tick, for the event to run
    bool isHeldAfterHit() const {
        return mHeldAfterHit;
    }

    // the tick after a held hit; the projectile is removed after it
    virtual void tickAfterHit(ServerNetworkHandler &owner) {
        (void) owner;
    }

protected:
    void holdAfterHit(const Vector3f &hitPosition) {
        mHeldAfterHit = true;
        setPosition(hitPosition);
        setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    }

private:
    bool mHeldAfterHit = false;
};

// minecraft:projectile on_hit, read from the entity's definition: impact_damage, catch_fire, mob_effect,
// particle_on_hit and the minecraft:explode its definition_event adds, as the game applies them
class DefinitionProjectileActor : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;

    // the explosion the hit added goes off now, its fuse being 0
    void tickAfterHit(ServerNetworkHandler &owner) override;

    // a homing projectile takes the shooter's motion and the target to steer to
    void launchAt(ServerNetworkHandler &owner, const Actor &shooter, const Actor &target);

    bool steer(ServerNetworkHandler &owner, Vector3f &motion) override;

    void afterMove(ServerNetworkHandler &owner) override;

    void reflect(Actor &attacker, const Vector3f &direction) override;

protected:
    // what the hit does to the actor or player struck, before anything that removes the projectile
    void _hitTarget(ServerNetworkHandler &owner, const Vector3f &hitPosition, Actor &target);

    // what the hit does wherever it lands: the block set alight, the particle, the sound, the explosion held
    void _hitAnywhere(ServerNetworkHandler &owner, const Vector3f &hitPosition, bool struckActor);

private:
    // null when the target is gone or dead
    Actor *_homingTarget(ServerNetworkHandler &owner) const;

    // the next leg of the flight: one block along an open axis toward the target, never along `axis`
    void _chooseDirection(ServerNetworkHandler &owner, int32_t axis);

    int64_t mHomingTarget = -1;
    Vector3f mTargetDelta;
    int32_t mFlightSteps = 0;
    // the axis of the current leg: 0 x, 1 y, 2 z, -1 none
    int32_t mAxis = -1;
};

class SnowballActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;
};

class IceBallActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;
};

class EggActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;

private:
    void _hatchChicks(ServerNetworkHandler &owner, const Vector3f &hitPosition) const;
};

class EnderPearlActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;
};

// the impact damage of its definition, then the wind burst
class WindChargeActor final : public DefinitionProjectileActor {
public:
    using DefinitionProjectileActor::DefinitionProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;

private:
    void _burst(ServerNetworkHandler &owner, const Vector3f &hitPosition);
};

class ExperienceBottleActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;
};

class PotionActor : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    int32_t getPotionId() const {
        return mPotionId;
    }

    void setPotionId(int32_t potionId) {
        mPotionId = potionId;
    }

private:
    int32_t mPotionId = 0;
};

class SplashPotionActor final : public PotionActor {
public:
    using PotionActor::PotionActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;
};

class LingeringPotionActor final : public PotionActor {
public:
    using PotionActor::PotionActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;
};

// flies straight, leaves dragon's breath where it lands (dragon_fireball.json: minecraft:projectile, spawn_aoe_cloud)
class DragonFireballActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    float getGravity() const override {
        return 0.0f;
    }

    float getInertia() const override {
        return 1.0f;
    }

    bool passesThroughOwner() const override {
        return true;
    }

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;

private:
    void _burst(ServerNetworkHandler &owner, const Vector3f &hitPosition);
};

// wither_skull.json, wither_skull_dangerous.json: on_hit gives the actor hit its mob_effect, then the
// definition_event explodes the skull with the minecraft:explode of the group it adds
class WitherSkullActor final : public DefinitionProjectileActor {
public:
    WitherSkullActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size);

    float getGravity() const override {
        return 0.0f;
    }

    float getInertia() const override {
        return 1.0f;
    }

    bool passesThroughOwner() const override {
        return true;
    }

    // minecraft:projectile is_dangerous: the blue skull
    bool isDangerous() const {
        return mDangerous;
    }

private:
    bool mDangerous = false;
};

class ArrowActor : public ProjectileActor {
public:
    using ProjectileActor::ProjectileActor;

    float getInertia() const override {
        return 0.99f;
    }
};

class ThrownTridentActor final : public ArrowActor {
public:
    using ArrowActor::ArrowActor;
};

class FireworksRocketActor : public ProjectileActor {
public:
    using ProjectileActor::ProjectileActor;

    // not a projectile to the game: the default snow shape
    bool collidesAtSnowLayerLook() const override {
        return false;
    }
};

class ShulkerBulletActor final : public DefinitionProjectileActor {
public:
    using DefinitionProjectileActor::DefinitionProjectileActor;

    bool hasGravity() const override {
        return false;
    }
};

#define FALCON_REGISTER_PROJECTILE(type, tag, identifier, width, height)                                               \
    static const ActorClassRegistry::Registration gActorRegistration##tag(                                             \
            (identifier), [](uint64_t runtimeId, const std::string &actorIdentifier) -> std::unique_ptr<ServerActor> { \
                return std::make_unique<type>(runtimeId, actorIdentifier, ActorSize{width, height});                   \
            })
