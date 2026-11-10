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

    // ActorType 0x48 has no projectile bit (sub_1436DD5C0): the default snow shape
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
