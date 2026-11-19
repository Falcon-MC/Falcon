#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class Level;
class ServerNetworkHandler;

class SkullBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Skull";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // degrees; only a skull standing on a floor shows it
    void setRotation(float rotation) {
        mRotation = rotation;
    }

    // the yaw of a skull placed on a floor, in 16 steps of 22.5 degrees
    static float floorRotation(float yaw);

    // creates the actor if the skull has none yet, sets its rotation and sends it to the viewers
    static void place(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                      float rotation);

private:
    void _putFields(Tag &data) const;

    float mRotation = 0.0f;
    // a value from before skulls were separate blocks; 255 when unset
    uint8_t mSkullType = 0xFF;
    bool mDoingAnimation = false;
    int32_t mMouthTickCount = 0;
};
