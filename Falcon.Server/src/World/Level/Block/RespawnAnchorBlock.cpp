#include "World/Level/Block/RespawnAnchorBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(RespawnAnchorBlock, 405);

namespace {
    const char *CHARGE = "respawn_anchor_charge";
    const char *GLOWSTONE = "minecraft:glowstone";
    // level sound events 308, 309, 310 (sub_141E293F0 and its neighbours)
    const char *SOUND_CHARGE = "respawn_anchor.charge";
    const char *SOUND_DEPLETE = "respawn_anchor.deplete";
    const char *SOUND_SET_SPAWN = "respawn_anchor.set_spawn";
    const double EXPLOSION_RADIUS = 5.0;
    const int WATER_SIDES_TO_SHIELD = 2;

    const int NEIGHBOURS[6][3] = {
            {0,  -1, 0},
            {0,  1,  0},
            {0,  0,  -1},
            {0,  0,  1},
            {-1, 0,  0},
            {1,  0,  0}
    };

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isSolidAt(Level &level, int32_t x, int32_t y, int32_t z) {
        if (y < level.getMinY() || y > level.getMaxY())
            return false;

        return BlockSupport::isSolid(level.getBlockState(x, y, z));
    }

    // solid floor, room for the feet and the head
    bool canStandAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return isSolidAt(level, x, y - 1, z) && !isSolidAt(level, x, y, z) && !isSolidAt(level, x, y + 1, z);
    }

    bool isWaterAt(Level &level, int32_t x, int32_t y, int32_t z) {
        if (y < level.getMinY() || y > level.getMaxY())
            return false;

        return LiquidView(level.getBlockState(x, y, z)).isWater()
               || LiquidView(level.getBlockStateAtLayer(x, y, z, 1)).isWater();
    }

    bool isGlowstone(const ItemStack &item) {
        return !item.isAir() && item.mDefinition != nullptr && item.mDefinition->getIdentifier() == GLOWSTONE;
    }

    bool holdsSpawnPoint(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
        return player.hasSpawnPoint() && player.getSpawnDimension() == DimensionType::Nether
               && player.getSpawnPoint() == position && player.getSpawnWorld() == owner.getWorldFor(player).getName();
    }
}

bool RespawnAnchorBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:respawn_anchor";
}

int RespawnAnchorBlock::getCharge(const BlockState &state) {
    return state.mStates.getInt(CHARGE, 0);
}

// sub_14A24D580: no neighbour update, sound at the center
void RespawnAnchorBlock::addCharge(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int delta) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    Tag states = state.mStates;
    states.putInt(CHARGE, std::clamp(getCharge(state) + delta, 0, MAX_CHARGE));
    level.setBlock(position, BlockState(state.mName, states), false);
    owner.playLevelSound(level, delta > 0 ? SOUND_CHARGE : SOUND_DEPLETE, centerOf(position));
}

// sub_14A24DD00
bool RespawnAnchorBlock::isChargedAnchor(Level &level, const Vector3i &position) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    return VanillaBlocks::getAs<RespawnAnchorBlock>(state.mName) != nullptr && getCharge(state) > 0;
}

// sub_14A24C210: two of the six neighbours in water keep the blast from breaking blocks
bool RespawnAnchorBlock::isSurroundedByWater(Level &level, const Vector3i &position) {
    int sides = 0;
    for (const int *offset: NEIGHBOURS) {
        if (isWaterAt(level, position.x + offset[0], position.y + offset[1], position.z + offset[2]))
            sides++;
    }

    return sides >= WATER_SIDES_TO_SHIELD;
}

// sub_14380F1E0: the position itself, else x-1..x+2 then z-1..z+2 on its layer, z fastest
std::optional<Vector3i> RespawnAnchorBlock::findStandUpPosition(Level &level, const Vector3i &position) {
    if (canStandAt(level, position.x, position.y, position.z))
        return position;

    for (int32_t x = position.x - 1; x <= position.x + 2; x++) {
        for (int32_t z = position.z - 1; z <= position.z + 2; z++) {
            if (canStandAt(level, x, position.y, z))
                return Vector3i(x, position.y, z);
        }
    }

    return std::nullopt;
}

// sub_1402DCF30: the anchor must still be charged and the spot standable, then a charge goes
std::optional<Vector3f> RespawnAnchorBlock::respawn(ServerNetworkHandler &owner, Level &level,
                                                    const Vector3i &anchor, const Vector3i &standPosition) {
    if (!isChargedAnchor(level, anchor))
        return std::nullopt;

    const std::optional<Vector3i> standUp = findStandUpPosition(level, standPosition);
    if (!standUp.has_value())
        return std::nullopt;

    addCharge(owner, level, anchor, -1);
    return Vector3f((float) standUp->x + 0.5f, (float) standUp->y, (float) standUp->z + 0.5f);
}

void RespawnAnchorBlock::explode(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    level.setBlock(position, BlockState("minecraft:air"), true);

    Explosion explosion(owner, level, centerOf(position), EXPLOSION_RADIUS, nullptr, false);
    explosion.setIncendiary(true);
    explosion.setInWater(isSurroundedByWater(level, position));
    explosion.explode();
}

// sub_14A24C210
bool RespawnAnchorBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const bool glowstone = isGlowstone(player.getInventory().getItemInHand());
    const int charge = getCharge(state);

    if (glowstone && charge < MAX_CHARGE) {
        player.consumeOneHeldItem();
        addCharge(owner, level, position, 1);
        return true;
    }

    // glowstone still counts as handled: it is not placed against the anchor
    if (charge <= 0)
        return glowstone;

    if (player.getDimension() != DimensionType::Nether) {
        if (level.getGameRules().getBool("respawnblocksexplode"))
            explode(owner, level, position);
        return true;
    }

    if (holdsSpawnPoint(owner, player, position))
        return glowstone;

    player.setSpawnPoint(position, player.getSpawnStandPositionHere(), DimensionType::Nether,
                         owner.getWorldFor(player).getName());
    owner.playLevelSound(level, SOUND_SET_SPAWN, centerOf(position));
    player.sendTranslation("§7%tile.respawn_anchor.respawnSet", {});
    return true;
}

// sub_14A24DF80: charge / 4 * 15, truncated
int RespawnAnchorBlock::getLightEmission(const BlockState &state, int dataEmission) const {
    (void) dataEmission;
    return (int) ((float) getCharge(state) / (float) MAX_CHARGE * 15.0f);
}
