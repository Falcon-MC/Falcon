#include "Block/Blocks/PortalBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockState.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/LavaBlock.h"
#include "Block/Blocks/PortalHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <string>

FALCON_REGISTER_BLOCK(PortalBlock, 5);

using namespace PortalHelpers;

namespace {
    const char *BEDROCK_IDENTIFIER = "minecraft:bedrock";
}

bool PortalBlock::matches(const std::string &identifier) {
    return identifier == PORTAL_IDENTIFIER;
}

bool PortalBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const bool alongX = stateText(state, "portal_axis") != "z";

    const Vector3i sides[4] = {
            Vector3i(position.x, position.y - 1, position.z),
            Vector3i(position.x, position.y + 1, position.z),
            alongX ? Vector3i(position.x - 1, position.y, position.z)
                   : Vector3i(position.x, position.y, position.z - 1),
            alongX ? Vector3i(position.x + 1, position.y, position.z)
                   : Vector3i(position.x, position.y, position.z + 1)
    };

    for (const Vector3i &side: sides) {
        const std::string identifier = identifierAt(level, side.x, side.y, side.z);
        if (identifier != PORTAL_IDENTIFIER && identifier != OBSIDIAN_IDENTIFIER)
            return false;
    }

    return true;
}

void PortalBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    if (canSurvive(level, position, state))
        return;

    BlockActionHandler::destroyBlock(owner, level, position, state, false, ItemStack::air());
}

void PortalBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                const BlockState &state) const {
    (void) position;
    (void) state;

    if (actor.getPortalCooldown() > 0)
        return;

    actor.setLastPortalTick(owner.getCurrentTick());

    const int32_t ticks = actor.getPortalTicks() + 1;
    actor.setPortalTicks(ticks);

    const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor);
    const bool instant = player != nullptr && player->getGameType() == (int32_t) GameType::Creative;
    if (ticks < (instant ? 1 : PORTAL_DELAY_TICKS))
        return;

    actor.setPortalTicks(0);

    Level &level = owner.getLevelFor(actor);
    const DimensionType current = level.getDimensionType();
    if (current != DimensionType::Overworld && current != DimensionType::Nether)
        return;

    const DimensionType target = current == DimensionType::Overworld
                                 ? DimensionType::Nether
                                 : DimensionType::Overworld;

    Level &destination = owner.getDimension(target);

    const Vector3f actorPosition = actor.getPosition();
    const Vector3i source((int32_t) std::floor(actorPosition.x),
                          (int32_t) std::floor(actorPosition.y),
                          (int32_t) std::floor(actorPosition.z));

    Vector3i landing(0, 0, 0);
    if (!findDestination(destination, source, landing))
        return;

    Vector3i existing(0, 0, 0);
    Vector3f arrival(0.0f, 0.0f, 0.0f);

    if (findNearestPortal(destination, landing, existing)) {
        arrival = Vector3f((float) existing.x + 0.5f, (float) existing.y, (float) existing.z + 0.5f);
    } else {
        spawnPortal(destination, landing, &owner);
        arrival = Vector3f((float) landing.x + 1.0f, (float) landing.y + 1.0f, (float) landing.z + 0.5f);
    }

    actor.setPortalCooldown(PORTAL_COOLDOWN_TICKS);
    owner.changeActorDimension(actor, target, arrival);
}

void PortalBlock::spawnPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
    int32_t x = position.x;
    int32_t y = position.y;
    int32_t z = position.z;

    const BlockState air((std::string(AIR_IDENTIFIER)));
    const BlockState obsidian((std::string(OBSIDIAN_IDENTIFIER)));
    const BlockState portal = makePortalState("x");

    for (int32_t offsetX = -2; offsetX <= 4; ++offsetX) {
        for (int32_t offsetY = -1; offsetY <= 5; ++offsetY) {
            for (int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                const Vector3i target(x + offsetX, y + offsetY, z + offsetZ);

                if (!isInsideLevel(level, target))
                    continue;

                if (identifierAt(level, target.x, target.y, target.z) == BEDROCK_IDENTIFIER)
                    continue;

                writeBlock(level, target, air, owner);
            }
        }
    }

    x -= 1;
    z -= 1;

    writeBlock(level, Vector3i(x + 1, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 2, y, z), obsidian, owner);

    z++;
    writeBlock(level, Vector3i(x, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 1, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 2, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 3, y, z), obsidian, owner);

    z++;
    writeBlock(level, Vector3i(x + 1, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 2, y, z), obsidian, owner);

    z--;

    for (int32_t i = 0; i < 3; ++i) {
        y++;
        writeBlock(level, Vector3i(x, y, z), obsidian, owner);
        writeBlock(level, Vector3i(x + 1, y, z), portal, owner);
        writeBlock(level, Vector3i(x + 2, y, z), portal, owner);
        writeBlock(level, Vector3i(x + 3, y, z), obsidian, owner);
    }

    y++;
    writeBlock(level, Vector3i(x, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 1, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 2, y, z), obsidian, owner);
    writeBlock(level, Vector3i(x + 3, y, z), obsidian, owner);
}

bool PortalBlock::findNearestPortal(Level &level, const Vector3i &origin, Vector3i &out) {
    const int32_t minChunkX = (origin.x - PORTAL_SEARCH_RADIUS) >> 4;
    const int32_t maxChunkX = (origin.x + PORTAL_SEARCH_RADIUS) >> 4;
    const int32_t minChunkZ = (origin.z - PORTAL_SEARCH_RADIUS) >> 4;
    const int32_t maxChunkZ = (origin.z + PORTAL_SEARCH_RADIUS) >> 4;

    const int32_t minY = std::max(level.getMinY(), (int32_t) LevelChunk::MIN_Y);
    const int32_t maxY = std::min(level.getMaxY(), (int32_t) LevelChunk::MAX_Y);

    bool found = false;
    int64_t bestDistance = 0;
    int64_t bestHeight = 0;

    for (int32_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
        for (int32_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ) {
            if (!level.isChunkResident(chunkX, chunkZ))
                continue;

            for (int32_t localX = 0; localX < 16; ++localX) {
                const int32_t x = (chunkX << 4) + localX;

                if (x < origin.x - PORTAL_SEARCH_RADIUS || x > origin.x + PORTAL_SEARCH_RADIUS)
                    continue;

                for (int32_t localZ = 0; localZ < 16; ++localZ) {
                    const int32_t z = (chunkZ << 4) + localZ;

                    if (z < origin.z - PORTAL_SEARCH_RADIUS || z > origin.z + PORTAL_SEARCH_RADIUS)
                        continue;

                    const int64_t deltaX = (int64_t) x - (int64_t) origin.x;
                    const int64_t deltaZ = (int64_t) z - (int64_t) origin.z;
                    const int64_t distance = deltaX * deltaX + deltaZ * deltaZ;

                    if (found && distance > bestDistance)
                        continue;

                    for (int32_t y = minY; y <= maxY; ++y) {
                        const BlockState *state = level.peekBlockPtr(x, y, z);
                        if (state == nullptr || state->mName != PORTAL_IDENTIFIER)
                            continue;

                        const BlockState *below = level.peekBlockPtr(x, y - 1, z);
                        if (below != nullptr && below->mName == PORTAL_IDENTIFIER)
                            continue;

                        const int64_t deltaY = (int64_t) origin.y - (int64_t) y;
                        const int64_t height = deltaY * deltaY;

                        if (!found || distance < bestDistance
                            || (distance == bestDistance && height < bestHeight)) {
                            found = true;
                            bestDistance = distance;
                            bestHeight = height;
                            out = Vector3i(x, y, z);
                        }
                    }
                }
            }
        }
    }

    return found;
}

bool PortalBlock::findDestination(Level &destination, const Vector3i &source, Vector3i &out) {
    const int32_t scale = (int32_t) Dimension::NETHER_COORDINATE_SCALE;
    const bool toNether = destination.getDimensionType() == DimensionType::Nether;

    int32_t x;
    int32_t z;

    if (toNether) {
        x = source.x / scale;
        z = source.z / scale;
    } else {
        x = source.x * scale;
        z = source.z * scale;
    }

    (void) destination.getChunk(x >> 4, z >> 4);

    int32_t y = destination.getHeightAt(x, z);
    if (toNether)
        y = std::min(y, NETHER_ROOF_LIMIT);

    for (int32_t i = y; i > destination.getMinY() + 2; --i) {
        const std::string ground = identifierAt(destination, x, i - 1, z);

        const bool space = isAirAt(destination, x, i, z)
                           && isAirAt(destination, x, i + 1, z)
                           && isAirAt(destination, x, i + 2, z)
                           && isAirAt(destination, x, i + 3, z)
                           && isAirAt(destination, x, i + 4, z);

        if (!space || !BlockSupport::isSolid(BlockState(ground)))
            continue;

        if (VanillaBlocks::getAs<LavaBlock>(ground) != nullptr)
            continue;

        if (toNether) {
            if (ground == BEDROCK_IDENTIFIER)
                continue;
        } else if (VanillaBlocks::getAs<WaterBlock>(ground) != nullptr) {
            continue;
        }

        y = i;
        break;
    }

    const int32_t clamped = std::max(destination.getMinY(), std::min(destination.getMaxY(), y));
    out = Vector3i(x, clamped + 1, z);
    return true;
}
