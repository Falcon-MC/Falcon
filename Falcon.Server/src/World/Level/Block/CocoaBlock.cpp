#include "World/Level/Block/CocoaBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/PlantGrowthHelpers.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(CocoaBlock, 319);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_COCOA_AGE = 2;
}

bool CocoaBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cocoa";
}

void CocoaBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) owner;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age >= MAX_COCOA_AGE || RandomTickSystem::nextInt(2) != 0)
        return;

    BlockChangeSystem::change(level, position, DecorationSupport::withState(state, AGE, age + 1),
                              BlockChangeCause::Grow, true);
}

bool CocoaBlock::getDrops(const BlockState &state, const ItemStack &, int32_t, std::vector<BlockDrop> &drops) const {
    drops.push_back({"minecraft:cocoa_beans", state.mStates.getInt(AGE) == MAX_COCOA_AGE ? 3 : 1});
    return true;
}

bool CocoaBlock::canPlaceAt(Level &level, const Vector3i &position, int face) const {
    if (face < PlacementOrientation::FACE_NORTH)
        return false;
    const Vector3i wood = BlockSupport::supportOf(position, face);
    const auto &name = level.getBlockState(wood.x, wood.y, wood.z).mName;
    return name == "minecraft:jungle_log" || name == "minecraft:stripped_jungle_log" || name == "minecraft:jungle_wood"
           || name == "minecraft:stripped_jungle_wood";
}

BlockState CocoaBlock::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    return DecorationSupport::withState(state, "direction", PlacementOrientation::horizontalOrdinal(context.mFace ^ 1));
}
