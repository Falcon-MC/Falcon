#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

// fields as the game loads and saves them
class StructureBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "StructureBlock";

    // StructureBlockType
    enum class Mode : int32_t { Data = 0, Save = 1, Load = 2, Corner = 3, Invalid = 4, Export = 5 };

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag getSpawnCompound() const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Mode mMode = Mode::Save;
    int32_t mRedstoneSaveMode = 0;
    Vector3i mOffset{0, -1, 0};
    Vector3i mSize{5, 5, 5};
    bool mPowered = false;
    std::string mStructureName;
    bool mHasFilteredName = false;
    std::string mFilteredName;
    std::string mDataField;
    bool mIgnoreEntities = false;
    bool mIncludePlayers = false;
    bool mRemoveBlocks = false;
    int64_t mLastTouchedPlayerId = 0;
    bool mShowBoundingBox = true;
    uint8_t mRotation = 0;
    uint8_t mMirror = 0;
    uint8_t mAnimationMode = 0;
    float mAnimationSeconds = 0.0f;
    float mIntegrity = 100.0f;
    int64_t mSeed = 0;

private:
    void _putFields(Tag &data) const;
};
