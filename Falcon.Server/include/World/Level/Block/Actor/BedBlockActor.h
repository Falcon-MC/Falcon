#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class BedBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Bed";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Container *getContainer() override {
        return nullptr;
    }

    int8_t getColor() const {
        return mColor;
    }

    void setColor(int8_t color) {
        mColor = color;
    }

    // one pet per bed sleeps with its owner; not saved
    bool claimPet(int64_t uniqueId);

    void releasePet(int64_t uniqueId);

private:
    static constexpr int64_t NO_PET = -1;

    int8_t mColor = 0;
    int64_t mPetId = NO_PET;
};
