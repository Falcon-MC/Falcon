#include "World/Item/MinecartItem.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Vehicle/MinecartActor.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Block/BaseRailBlock.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Level.h"

FALCON_REGISTER_ITEM(MinecartItem, 100);

namespace {
    // the minecart sits a sixteenth above the rail, half a block higher on a slope
    const float RAIL_HEIGHT = 0.0625f;
    const float SLOPE_HEIGHT = 0.5f;
    // a dispensed minecart lands this far out of the dispenser, a little above or below the rail it meets
    const float DISPENSE_REACH = 1.125f;
    const float DISPENSE_ON_RAIL = 0.1f;
    const float DISPENSE_ON_SLOPE = 0.6f;
    const float DISPENSE_DOWN_RAIL = -0.9f;
    const float DISPENSE_DOWN_SLOPE = -0.4f;
    // facing_direction: down, up, north, south, west, east
    const int32_t STEPS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    bool isSlope(const BlockState &rail) {
        const int32_t direction = rail.mStates.getInt("rail_direction", 0);
        return direction >= BaseRailBlock::ASCENDING_EAST && direction <= BaseRailBlock::ASCENDING_SOUTH;
    }
}

MinecartItem::MinecartItem(const Item &base) : Item(base) {}

bool MinecartItem::matches(const std::string &identifier) {
    return !MinecartActor::actorOfItem(identifier).empty();
}

bool MinecartItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;
    if (item.mDefinition == nullptr || player.isSpectator())
        return false;

    Level &level = owner.getLevelFor(player);
    const BlockState *rail = level.peekBlockPtr(blockPosition.x, blockPosition.y, blockPosition.z);
    if (rail == nullptr || !BaseRailBlock::matches(rail->mName))
        return false;

    const int32_t direction = rail->mStates.getInt("rail_direction", 0);
    const bool slope = direction >= BaseRailBlock::ASCENDING_EAST && direction <= BaseRailBlock::ASCENDING_SOUTH;
    const Vector3f position((float) blockPosition.x + 0.5f,
                            (float) blockPosition.y + RAIL_HEIGHT + (slope ? SLOPE_HEIGHT : 0.0f),
                            (float) blockPosition.z + 0.5f);
    if (owner.spawnActor(level, MinecartActor::actorOfItem(item.mDefinition->getIdentifier()), position) == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}

// a rail in front of the dispenser, or one block below an empty space there, takes the minecart; anything else
// drops the item
bool MinecartItem::dispense(const DispenseSource &source) const {
    const ItemStack &item = source.getItem();
    if (item.mDefinition == nullptr || source.mFace < 0 || source.mFace >= 6)
        return false;

    const int32_t *step = STEPS[source.mFace];
    const Vector3i front(source.mBlockPosition.x + step[0], source.mBlockPosition.y + step[1],
                         source.mBlockPosition.z + step[2]);
    const BlockState *ahead = source.mLevel.peekBlockPtr(front.x, front.y, front.z);
    if (ahead == nullptr)
        return false;

    float lift;
    if (BaseRailBlock::matches(ahead->mName)) {
        lift = isSlope(*ahead) ? DISPENSE_ON_SLOPE : DISPENSE_ON_RAIL;
    } else {
        const BlockState *below = source.mLevel.peekBlockPtr(front.x, front.y - 1, front.z);
        if (ahead->mName != "minecraft:air" || below == nullptr || !BaseRailBlock::matches(below->mName))
            return false;
        lift = source.mFace != 0 && isSlope(*below) ? DISPENSE_DOWN_SLOPE : DISPENSE_DOWN_RAIL;
    }

    const Vector3f position((float) source.mBlockPosition.x + 0.5f + (float) step[0] * DISPENSE_REACH,
                            (float) (source.mBlockPosition.y + step[1]) + lift,
                            (float) source.mBlockPosition.z + 0.5f + (float) step[2] * DISPENSE_REACH);
    if (source.mOwner.spawnActor(source.mLevel, MinecartActor::actorOfItem(item.mDefinition->getIdentifier()),
                                 position)
        == nullptr)
        return false;

    source.removeOne();
    source.playClick();
    return true;
}
