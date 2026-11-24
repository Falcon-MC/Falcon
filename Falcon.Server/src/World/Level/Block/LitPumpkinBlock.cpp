#include "World/Level/Block/LitPumpkinBlock.h"

#include "World/Actor/Mob/Neutral/GolemBuilding.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"

FALCON_REGISTER_BLOCK(LitPumpkinBlock, 205);

bool LitPumpkinBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::LIT_PUMPKIN;
}

void LitPumpkinBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) state;
    GolemBuilding::tryBuild(owner, level, position);
}
