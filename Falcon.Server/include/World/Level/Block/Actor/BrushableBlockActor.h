#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Inventory/SimpleContainerInventory.h"

#include <cstdint>
#include <string>

class Level;
class ServerNetworkHandler;

// BrushableBlockActor: suspicious sand and gravel; the item it gives up, how far it is
// brushed and from which face
class BrushableBlockActor final : public BlockActor {
public:
    // brushes it takes to dig the item out
    static const int32_t BRUSHES_TO_COMPLETE = 10;

    static constexpr const char *BLOCK_ACTOR_ID = "BrushableBlock";
    // the table the block makes its actor with
    static constexpr const char *EMPTY_LOOT_TABLE = "loot_tables/entities/empty_brushable_block.json";

    BrushableBlockActor();

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Container *getContainer() override {
        return &mInventory;
    }

    // a brush stroke without the world: false while the stroke cooldown runs;
    // a counted stroke rolls the loot first, then completes the block at BRUSHES_TO_COMPLETE
    bool stroke(int64_t tick, uint8_t face, bool &rollLoot);

    // BrushableBlockActor::checkReset: the delay of the next block tick, 0 for none;
    // `changed`: the count fell back
    int32_t checkReset(int64_t tick, bool &changed);

    // the brushed_progress the block shows for a brush count
    static int32_t progressOf(int32_t brushCount);

    // the item pops out of the brushed face, the block turns plain; this actor goes with it
    void complete(ServerNetworkHandler &owner, Level &level);

    // the block state and the clients follow the count
    void sync(ServerNetworkHandler &owner, Level &level) const;

    const std::string &getType() const {
        return mType;
    }

    // the block it was made for, saved as "type"
    void setType(const std::string &blockName) {
        mType = blockName;
    }

    int32_t getBrushCount() const {
        return mBrushCount;
    }

    uint8_t getBrushDirection() const {
        return mBrushDirection;
    }

private:
    void _putFields(Tag &data) const;

    std::string mType;
    int32_t mBrushCount = 0;
    // a face index; 6 until a brush first touches it
    uint8_t mBrushDirection = 6;
    SimpleContainerInventory mInventory{1};
    // left out of the save
    bool mLootRolled = false;
    bool mStrokeTimed = false;
    int64_t mNextStrokeTick = 0;
    bool mResetTimed = false;
    int64_t mResetTick = 0;
};
