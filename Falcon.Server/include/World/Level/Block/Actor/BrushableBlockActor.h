#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>
#include <string>

// BrushableBlockActor (ctor sub_143306C90, 26.60): suspicious sand and gravel; the item it gives up, how far it is
// brushed and from which face
class BrushableBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "BrushableBlock";
    // the table the block makes its actor with (sub_147154390, loot type 6)
    static constexpr const char *EMPTY_LOOT_TABLE = "loot_tables/entities/empty_brushable_block.json";

    BrushableBlockActor();

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

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
    // the saved item stack, kept as written
    Tag mItem;
    bool mHasItem = false;
};
