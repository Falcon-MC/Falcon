#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class ItemStack;
class ServerNetworkHandler;
class ServerPlayer;

class BannerBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Banner";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // the reverse of the banner's asItemInstance: the item's data value is the base color, its other
    // tag fields the banner's saved ones
    static void placeFromItem(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state, const ItemStack &item);

    // the dye index counted from black, as the banner item's damage
    void setBase(int32_t base) {
        mBase = base;
    }

    void setType(int32_t type) {
        mType = type;
    }

private:
    void _putFields(Tag &data) const;

    int32_t mBase = 0;
    // 1 for the ominous banner
    int32_t mType = 0;
    Tag mPatterns = Tag::ofList(Tag::Type::Compound);
};
