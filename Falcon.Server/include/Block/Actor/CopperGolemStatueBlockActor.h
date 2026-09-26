#pragma once

#include "Block/BlockActor.h"

#include <cstdint>

class CopperGolemStatueBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "CopperGolemStatue";
    static const int32_t POSE_COUNT = 4;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    int32_t getPose() const {
        return mPose;
    }

    void setPose(int32_t pose);

    bool hasStoredActor() const {
        return mStoredActor.isCompound();
    }

    const Tag &getStoredActor() const {
        return mStoredActor;
    }

    void setStoredActor(const Tag &data) {
        mStoredActor = data;
    }

private:
    int32_t mPose = 0;
    Tag mStoredActor;
};
