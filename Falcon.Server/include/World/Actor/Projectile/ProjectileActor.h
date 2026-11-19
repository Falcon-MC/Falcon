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

class WindChargeActor final : public ThrownProjectileActor {
public:
    using ThrownProjectileActor::ThrownProjectileActor;

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;
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
class WitherSkullActor final : public ThrownProjectileActor {
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

    bool onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) override;

    bool onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) override;

    // the explode component added by the hit goes off now, its fuse being 0
    void tickAfterHit(ServerNetworkHandler &owner) override;

private:
    void _applyEffects(ServerNetworkHandler &owner, Actor &target) const;

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

    // ActorType 0x48 has no projectile bit: the default snow shape
    bool collidesAtSnowLayerLook() const override {
        return false;
    }
};

class ShulkerBulletActor : public ProjectileActor {
public:
    using ProjectileActor::ProjectileActor;

    bool hasGravity() const override {
        return false;
    }
};

#define FALCON_REGISTER_PROJECTILE(type, tag, identifier, width, height)                                               \
    static const ActorClassRegistry::Registration gActorRegistration##tag(                                             \
            (identifier), [](uint64_t runtimeId, const std::string &actorIdentifier) -> std::unique_ptr<ServerActor> { \
                return std::make_unique<type>(runtimeId, actorIdentifier, ActorSize{width, height});                   \
            })
