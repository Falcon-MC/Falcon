#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class BannerBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Banner";

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // the dye index counted from black, as the banner item's damage
    void setBase(int32_t base) { mBase = base; }

    int32_t getBase() const { return mBase; }

private:
    void _putFields(Tag &data) const;

    int32_t mBase = 0;
    // 1 for the ominous banner
    int32_t mType = 0;
    Tag mPatterns = Tag::ofList(Tag::Type::Compound);
};
