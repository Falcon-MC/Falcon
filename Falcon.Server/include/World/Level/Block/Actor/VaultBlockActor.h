#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Level/Block/Actor/BlockActor.h"

#include <random>
#include <unordered_set>
#include <vector>

class ServerPlayer;

class VaultBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Vault";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }
    Tag saveNbt() const override;
    Tag getSpawnCompound() const override;
    void loadNbt(const Tag &data, const PacketCodecContext &context) override;
    void loadVaultData(const Tag &data);
    bool tick(ServerNetworkHandler &owner) override;
    bool unlock(ServerNetworkHandler &owner, ServerPlayer &player);

private:
    const BlockState *_block() const;
    bool _eligible(const ServerPlayer &player, float range) const;
    void _setState(const BlockState &block, const char *phase);
    void _setDisplay(ServerNetworkHandler &owner, Tag item);
    std::vector<Tag> _roll(ServerNetworkHandler &owner, const BlockState &block, bool preview);
    std::string _key(const BlockState &block) const;
    Vector3f _center() const;

    Tag mConfig = Tag::ofCompound();
    Tag mDisplay;
    Tag mConnected = Tag::ofList(Tag::Type::Long);
    std::vector<Tag> mRewards;
    std::unordered_set<int64_t> mRewarded;
    std::unordered_set<std::string> mRewardedAccounts;
    std::mt19937 mRandom{std::random_device{}()};
    int32_t mDelay = 0;
};
