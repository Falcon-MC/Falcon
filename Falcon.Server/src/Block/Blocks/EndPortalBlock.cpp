#include "Block/Blocks/EndPortalBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockState.h"
#include "Block/Blocks/PortalBlock.h"
#include "Block/Blocks/PortalHelpers.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <string>

FALCON_REGISTER_BLOCK(EndPortalBlock, 6);

using namespace PortalHelpers;

bool EndPortalBlock::matches(const std::string &identifier) {
    return identifier == END_PORTAL_IDENTIFIER;
}

bool EndPortalBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;

    const Vector3i sides[4] = {
            Vector3i(position.x - 1, position.y, position.z),
            Vector3i(position.x + 1, position.y, position.z),
            Vector3i(position.x, position.y, position.z - 1),
            Vector3i(position.x, position.y, position.z + 1)
    };

    for (const Vector3i &side: sides) {
        const std::string identifier = identifierAt(level, side.x, side.y, side.z);
        if (identifier != END_PORTAL_IDENTIFIER && identifier != END_PORTAL_FRAME_IDENTIFIER)
            return false;
    }

    return true;
}

void EndPortalBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    if (canSurvive(level, position, state))
        return;

    BlockActionHandler::destroyBlock(owner, level, position, state, false, ItemStack::air());
}

void EndPortalBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                   const BlockState &state) const {
    (void) position;
    (void) state;

    if (actor.getPortalCooldown() > 0)
        return;

    actor.setLastPortalTick(owner.getCurrentTick());
    actor.setPortalTicks(0);

    Level &level = owner.getLevelFor(actor);
    const DimensionType current = level.getDimensionType();

    if (current != DimensionType::TheEnd) {
        Level &end = owner.getDimension(DimensionType::TheEnd);
        spawnObsidianPlatform(end, Vector3i(END_PLATFORM_X, END_PLATFORM_Y, END_PLATFORM_Z), &owner);

        actor.setPortalCooldown(PortalBlock::PORTAL_COOLDOWN_TICKS);
        owner.changeActorDimension(actor, DimensionType::TheEnd,
                                    Vector3f((float) END_PLATFORM_X + 0.5f,
                                             (float) END_PLATFORM_Y + 1.0f,
                                             (float) END_PLATFORM_Z + 0.5f));
        return;
    }

    Level &overworld = owner.getDimension(DimensionType::Overworld);

    actor.setPortalCooldown(PortalBlock::PORTAL_COOLDOWN_TICKS);
    owner.changeActorDimension(actor, DimensionType::Overworld, overworld.getSpawnPositionForPlayer());
}

void EndPortalBlock::spawnObsidianPlatform(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
    const BlockState air((std::string(AIR_IDENTIFIER)));
    const BlockState obsidian((std::string(OBSIDIAN_IDENTIFIER)));

    for (int32_t blockX = position.x - 2; blockX <= position.x + 2; ++blockX) {
        for (int32_t blockZ = position.z - 2; blockZ <= position.z + 2; ++blockZ) {
            writeBlock(level, Vector3i(blockX, position.y - 1, blockZ), obsidian, owner);

            for (int32_t blockY = position.y; blockY <= position.y + 3; ++blockY) {
                writeBlock(level, Vector3i(blockX, blockY, blockZ), air, owner);
            }
        }
    }
}
