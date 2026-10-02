#include "Block/Blocks/PistonBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Systems/PistonSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(PistonBlock, 150);

bool PistonBlock::matches(const std::string &identifier) {
    return PistonSystem::isPiston(identifier);
}

void PistonBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const {
    PistonSystem::onBlockBroken(owner, level, position, state);
}
