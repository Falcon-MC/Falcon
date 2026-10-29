#include "World/Level/Block/PistonBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(PistonBlock, 150);

bool PistonBlock::matches(const std::string &identifier) {
    return PistonSystem::isPiston(identifier);
}

void PistonBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const {
    PistonSystem::onBlockBroken(owner, level, position, state);
}
