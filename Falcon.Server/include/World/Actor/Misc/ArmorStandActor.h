#pragma once

#include "World/Actor/Mob/MobEquipment.h"
#include "World/Actor/ServerActor.h"

class ArmorStandActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:armor_stand";

    static constexpr int32_t POSE_COUNT = 13;

    static constexpr float MAX_HEALTH = 6.0f;

    static constexpr int32_t LEVEL_EVENT_BREAK = 1060;

    static constexpr int32_t LEVEL_EVENT_HIT = 1061;

    static constexpr int32_t LEVEL_EVENT_LAND = 1062;

    static constexpr int32_t LEVEL_EVENT_PLACE = 1063;

    ArmorStandActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override {
        return ActorSize{0.5f, 1.975f};
    }

    bool isExpired() const override {
        return mExpired;
    }

    bool isPushable() const override {
        return false;
    }

    bool stopsProjectiles() const override {
        return true;
    }

    bool shouldSave() const override {
        return !mExpired && ServerActor::shouldSave();
    }

    void tick(ServerNetworkHandler &owner) override;

    bool onInteractAt(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &clickPosition) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    void sendEquipmentTo(ServerNetworkHandler &owner, const ServerPlayer &player) const override;

    void loadEquipment(const Tag &data, const PacketCodecContext &context) override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    int32_t getPoseIndex() const {
        return mPoseIndex;
    }

    // -1 for none; the chest band gives way to the main hand while the stand holds something
    static int slotForClick(float offsetY, bool mainhandOccupied);

    // false changes nothing; a creative player keeps the held stack when filling an empty slot
    static bool exchange(ItemStack &slot, ItemStack &held, bool creative);

private:
    enum class HitKind {
        Attack,
        Explosion,
        Fire,
        Void,
        Fall,
        Other
    };

    void _setPose(ServerNetworkHandler &owner, int32_t poseIndex);

    void _tickRedstone(ServerNetworkHandler &owner);

    void _tickFireContact(ServerNetworkHandler &owner);

    void _resync(ServerNetworkHandler &owner, ServerPlayer &player) const;

    bool _canInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &clickPosition) const;

    int _slotFor(const ItemStack &held, float offsetY) const;

    void _onAttacked(ServerNetworkHandler &owner, ServerPlayer *source);

    void _burn(ServerNetworkHandler &owner, float amount);

    void _shake(ServerNetworkHandler &owner);

    void _tickShake(ServerNetworkHandler &owner);

    void _break(ServerNetworkHandler &owner, bool drops);

    void _destroy(ServerNetworkHandler &owner, bool dropStand, bool dropEquipment);

    MobEquipment mEquipment;
    int32_t mPoseIndex = 0;
    int32_t mLastSignal = 0;
    int32_t mHurtTime = 0;
    int64_t mLastHitTick = 0;
    int64_t mLastInteractTick = -1;
    bool mExpired = false;
    HitKind mPendingHit = HitKind::Other;
    bool mPendingProjectile = false;
    bool mPendingMobAttacker = false;
};
