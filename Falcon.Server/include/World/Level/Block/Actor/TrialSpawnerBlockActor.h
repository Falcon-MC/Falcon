#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Storage/Loot/LootContext.h"

class Actor;
class ServerActor;

class TrialSpawnerBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "TrialSpawner";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;
    Tag getSpawnCompound() const override;
    bool tick(ServerNetworkHandler &owner) override;
    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    void loadTrialData(const Tag &data);
    bool configureFromSpawnEgg(ServerNetworkHandler &owner, const std::string &identifier);
    static Tag createTrialData(const Tag &configuration);
    static ServerActor *spawnMob(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                 const Tag &spawnData);
    static Vector3f ominousAnchor(const Actor &target);
    static std::optional<Vector3f> ominousPosition(Level &level, const Vector3f &anchor, int32_t height);
    static ServerActor *spawnOminousProjectile(ServerNetworkHandler &owner, Level &level,
                                                const Vector3f &position, const LootDrop &drop);
    static std::vector<LootDrop> rollOminousItems(const Tag &config, std::mt19937 &random);
    static Tag rollSpawnData(const Tag &config, std::mt19937 &random);
    static std::vector<LootDrop> rollReward(const Tag &config, std::mt19937 &random);

private:
    std::mt19937 mRandom{std::random_device{}()};
    Tag mTrialData = Tag::ofCompound();
};
