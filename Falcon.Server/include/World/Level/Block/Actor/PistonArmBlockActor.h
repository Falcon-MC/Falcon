#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <vector>

class PistonArmBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "PistonArm";

    static const int8_t STATE_RETRACTED = 0;
    static const int8_t STATE_EXTENDING = 1;
    static const int8_t STATE_EXTENDED = 2;
    static const int8_t STATE_RETRACTING = 3;

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    bool isMoving() const { return !mFinished; }

    void beginMove(bool extending, std::vector<Vector3i> attachedBlocks);

    void advance();

    void finish();

    const std::vector<Vector3i> &getAttachedBlocks() const { return mAttachedBlocks; }

    bool isExtending() const { return mExtending; }

    float getProgress() const { return mProgress; }

    void setSticky(bool sticky) { mSticky = sticky; }

    void setFacing(int facing) { mFacing = facing; }

    int getFacing() const { return mFacing; }

private:
    std::vector<Vector3i> mAttachedBlocks;
    float mProgress = 1.0f;
    float mLastProgress = 1.0f;
    int mFacing = 1;
    int8_t mState = STATE_RETRACTED;
    int8_t mNewState = STATE_RETRACTED;
    bool mSticky = false;
    bool mExtending = false;
    bool mFinished = true;
};
