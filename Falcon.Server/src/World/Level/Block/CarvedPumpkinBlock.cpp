#include "World/Level/Block/CarvedPumpkinBlock.h"

#include "World/Actor/Mob/Neutral/GolemBuilding.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CarvedPumpkinBlock, 205);

bool CarvedPumpkinBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:carved_pumpkin";
}

void CarvedPumpkinBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    (void) state;
    GolemBuilding::tryBuild(owner, level, position);
}
