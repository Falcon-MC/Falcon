#include "Block/Blocks/PortalBlocks.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockState.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/LavaBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <string>

FALCON_REGISTER_BLOCK(PortalBlock, 5);
FALCON_REGISTER_BLOCK(EndPortalBlock, 6);
FALCON_REGISTER_BLOCK(EndPortalFrameBlock, 7);
FALCON_REGISTER_BLOCK(ObsidianBlock, 8);

namespace {
    const char *AIR_IDENTIFIER = "minecraft:air";
    const char *OBSIDIAN_IDENTIFIER = "minecraft:obsidian";
    const char *PORTAL_IDENTIFIER = "minecraft:portal";
    const char *END_PORTAL_IDENTIFIER = "minecraft:end_portal";
    const char *END_PORTAL_FRAME_IDENTIFIER = "minecraft:end_portal_frame";
    const char *ENDER_EYE_IDENTIFIER = "minecraft:ender_eye";
    const char *BEDROCK_IDENTIFIER = "minecraft:bedrock";

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isInsideLevel(Level &level, const Vector3i &position) {
        if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y)
            return false;

        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }

    std::string identifierAt(Level &level, int32_t x, int32_t y, int32_t z) {
        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            return std::string(AIR_IDENTIFIER);

        return level.getBlockState(x, y, z).mName;
    }

    bool isAirAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return identifierAt(level, x, y, z) == AIR_IDENTIFIER;
    }

    bool isObsidianAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return identifierAt(level, x, y, z) == OBSIDIAN_IDENTIFIER;
    }

    int32_t stateFlag(const BlockState &state, const std::string &key) {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr)
            return 0;

        if (tag->getType() == Tag::Type::Byte)
            return tag->asByte();

        if (tag->getType() == Tag::Type::Int)
            return tag->asInt();

        return 0;
    }

    std::string stateText(const BlockState &state, const std::string &key) {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr || tag->getType() != Tag::Type::String)
            return std::string();

        return tag->asString();
    }

    BlockState makePortalState(const char *axis) {
        Tag states = Tag::ofCompound();
        states.putString("portal_axis", std::string(axis));
        return BlockState(std::string(PORTAL_IDENTIFIER), states);
    }

    void writeBlock(Level &level, const Vector3i &position, const BlockState &state, ServerNetworkHandler *owner) {
        if (!isInsideLevel(level, position))
            return;

        level.setBlockState(position.x, position.y, position.z, state);

        if (owner != nullptr)
            BlockActionHandler::broadcastBlockUpdate(*owner, level, position, state);
    }

    const char *expectedFrameFacing(int32_t x, int32_t z) {
        if (x == -2)
            return "east";

        if (x == 2)
            return "west";

        if (z == -2)
            return "south";

        return "north";
    }
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

bool EndPortalFrameBlock::matches(const std::string &identifier) {
    return identifier == END_PORTAL_FRAME_IDENTIFIER;
}

bool EndPortalFrameBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                     const BlockState &state) const {
    PlayerInventory &inventory = player.getInventory();
    const ItemStack &held = inventory.getItemInHand();

    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    if (held.mDefinition->getIdentifier() != ENDER_EYE_IDENTIFIER)
        return false;

    if (stateFlag(state, "end_portal_eye_bit") != 0)
        return false;

    Level &level = owner.getLevelFor(player);
    if (!isInsideLevel(level, position))
        return false;

    Tag states = state.mStates;
    states.putByte("end_portal_eye_bit", 1);

    const BlockState filled(state.mName, states);
    writeBlock(level, position, filled, &owner);

    owner.playNamedSound(level, "block.end_portal_frame.fill", centerOf(position), 1.0f, 1.0f);

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack updated = inventory.getItemInHand();
        updated.mCount -= 1;

        if (updated.mCount <= 0)
            inventory.setItemInHand(ItemStack::air());
        else
            inventory.setItemInHand(std::move(updated));

        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    tryCompletePortal(level, position, &owner);
    return true;
}

bool EndPortalFrameBlock::tryCompletePortal(Level &level, const Vector3i &framePosition,
                                            ServerNetworkHandler *owner) {
    int32_t minX = 0;
    int32_t minZ = 0;
    bool hasFrame = false;

    for (int32_t x = -4; x <= 4; ++x) {
        for (int32_t z = -4; z <= 4; ++z) {
            if (identifierAt(level, framePosition.x + x, framePosition.y, framePosition.z + z)
                != END_PORTAL_FRAME_IDENTIFIER)
                continue;

            if (!hasFrame) {
                hasFrame = true;
                minX = framePosition.x + x;
                minZ = framePosition.z + z;
                continue;
            }

            minX = std::min(minX, framePosition.x + x);
            minZ = std::min(minZ, framePosition.z + z);
        }
    }

    if (!hasFrame)
        return false;

    const Vector3i center(minX + 2, framePosition.y, minZ + 2);

    for (int32_t x = -2; x <= 2; ++x) {
        for (int32_t z = -2; z <= 2; ++z) {
            if ((x == -2 || x == 2) && (z == -2 || z == 2))
                continue;

            if (x != -2 && x != 2 && z != -2 && z != 2)
                continue;

            const Vector3i target(center.x + x, center.y, center.z + z);
            if (!isInsideLevel(level, target))
                return false;

            const BlockState state = level.getBlockState(target.x, target.y, target.z);
            if (state.mName != END_PORTAL_FRAME_IDENTIFIER)
                return false;

            if (stateFlag(state, "end_portal_eye_bit") == 0)
                return false;

            if (stateText(state, "minecraft:cardinal_direction") != expectedFrameFacing(x, z))
                return false;
        }
    }

    const BlockState endPortal((std::string(END_PORTAL_IDENTIFIER)));

    for (int32_t x = -1; x <= 1; ++x) {
        for (int32_t z = -1; z <= 1; ++z) {
            writeBlock(level, Vector3i(center.x + x, center.y, center.z + z), endPortal, owner);
        }
    }

    if (owner != nullptr)
        owner->playNamedSound(level, "block.end_portal.spawn", centerOf(center), 1.0f, 1.0f);

    return true;
}

bool ObsidianBlock::matches(const std::string &identifier) {
    return identifier == OBSIDIAN_IDENTIFIER;
}

bool ObsidianBlock::tryLightPortal(Level &level, const Vector3i &firePosition, ServerNetworkHandler *owner) {
    if (level.getDimensionType() == DimensionType::TheEnd)
        return false;

    if (lightPortalAtBase(level, firePosition, owner))
        return true;

    for (int32_t offset = 0; offset < PortalBlock::MAX_PORTAL_SIZE; ++offset) {
        const Vector3i below(firePosition.x, firePosition.y - offset, firePosition.z);
        const std::string identifier = identifierAt(level, below.x, below.y, below.z);

        if (identifier == OBSIDIAN_IDENTIFIER)
            return lightPortalAtBase(level, below, owner);

        if (identifier != AIR_IDENTIFIER)
            return false;
    }

    return false;
}

bool ObsidianBlock::lightPortalAtBase(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
    const int32_t targetX = position.x;
    const int32_t targetY = position.y;
    const int32_t targetZ = position.z;

    if (!isObsidianAt(level, targetX, targetY, targetZ))
        return false;

    for (int32_t i = 1; i < 4; ++i) {
        if (!isAirAt(level, targetX, targetY + i, targetZ))
            return false;
    }

    const int32_t maxSize = PortalBlock::MAX_PORTAL_SIZE;

    int32_t sizePosX = 0;
    int32_t sizeNegX = 0;
    int32_t sizePosZ = 0;
    int32_t sizeNegZ = 0;

    for (int32_t i = 1; i < maxSize; ++i) {
        if (!isObsidianAt(level, targetX + i, targetY, targetZ))
            break;

        ++sizePosX;
    }

    for (int32_t i = 1; i < maxSize; ++i) {
        if (!isObsidianAt(level, targetX - i, targetY, targetZ))
            break;

        ++sizeNegX;
    }

    for (int32_t i = 1; i < maxSize; ++i) {
        if (!isObsidianAt(level, targetX, targetY, targetZ + i))
            break;

        ++sizePosZ;
    }

    for (int32_t i = 1; i < maxSize; ++i) {
        if (!isObsidianAt(level, targetX, targetY, targetZ - i))
            break;

        ++sizeNegZ;
    }

    const int32_t sizeX = sizePosX + sizeNegX + 1;
    const int32_t sizeZ = sizePosZ + sizeNegZ + 1;

    if (sizeX >= 2 && sizeX <= maxSize) {
        int32_t scanX = targetX;
        const int32_t scanY = targetY + 1;
        const int32_t scanZ = targetZ;

        for (int32_t i = 0; i < sizePosX + 1; ++i) {
            if (!isAirAt(level, scanX + i, scanY, scanZ))
                return false;

            if (isObsidianAt(level, scanX + i + 1, scanY, scanZ)) {
                scanX += i;
                break;
            }
        }

        if (!isObsidianAt(level, scanX + 1, scanY, scanZ))
            return false;

        int32_t innerWidth = 0;
        for (int32_t i = 0; i < maxSize - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX - i, scanY, scanZ);

            if (identifier == AIR_IDENTIFIER) {
                ++innerWidth;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        int32_t innerHeight = 0;
        for (int32_t i = 0; i < maxSize - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX, scanY + i, scanZ);

            if (identifier == AIR_IDENTIFIER) {
                ++innerHeight;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        if (!(innerWidth <= maxSize - 2 && innerWidth >= 2
              && innerHeight <= maxSize - 2 && innerHeight >= 3))
            return false;

        for (int32_t height = 0; height < innerHeight + 1; ++height) {
            if (height == innerHeight) {
                for (int32_t width = 0; width < innerWidth; ++width) {
                    if (!isObsidianAt(level, scanX - width, scanY + height, scanZ))
                        return false;
                }

                continue;
            }

            if (!isObsidianAt(level, scanX + 1, scanY + height, scanZ)
                || !isObsidianAt(level, scanX - innerWidth, scanY + height, scanZ))
                return false;

            for (int32_t width = 0; width < innerWidth; ++width) {
                if (!isAirAt(level, scanX - width, scanY + height, scanZ))
                    return false;
            }
        }

        const BlockState portal = makePortalState("x");
        for (int32_t height = 0; height < innerHeight; ++height) {
            for (int32_t width = 0; width < innerWidth; ++width) {
                writeBlock(level, Vector3i(scanX - width, scanY + height, scanZ), portal, owner);
            }
        }

        if (owner != nullptr)
            owner->playLevelSound(level, LevelSoundEvent::FIRE_IGNITE, centerOf(position));

        return true;
    }

    if (sizeZ >= 2 && sizeZ <= maxSize) {
        const int32_t scanX = targetX;
        const int32_t scanY = targetY + 1;
        int32_t scanZ = targetZ;

        for (int32_t i = 0; i < sizePosZ + 1; ++i) {
            if (!isAirAt(level, scanX, scanY, scanZ + i))
                return false;

            if (isObsidianAt(level, scanX, scanY, scanZ + i + 1)) {
                scanZ += i;
                break;
            }
        }

        if (!isObsidianAt(level, scanX, scanY, scanZ + 1))
            return false;

        int32_t innerWidth = 0;
        for (int32_t i = 0; i < maxSize - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX, scanY, scanZ - i);

            if (identifier == AIR_IDENTIFIER) {
                ++innerWidth;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        int32_t innerHeight = 0;
        for (int32_t i = 0; i < maxSize - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX, scanY + i, scanZ);

            if (identifier == AIR_IDENTIFIER) {
                ++innerHeight;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        if (!(innerWidth <= maxSize - 2 && innerWidth >= 2
              && innerHeight <= maxSize - 2 && innerHeight >= 3))
            return false;

        for (int32_t height = 0; height < innerHeight + 1; ++height) {
            if (height == innerHeight) {
                for (int32_t width = 0; width < innerWidth; ++width) {
                    if (!isObsidianAt(level, scanX, scanY + height, scanZ - width))
                        return false;
                }

                continue;
            }

            if (!isObsidianAt(level, scanX, scanY + height, scanZ + 1)
                || !isObsidianAt(level, scanX, scanY + height, scanZ - innerWidth))
                return false;

            for (int32_t width = 0; width < innerWidth; ++width) {
                if (!isAirAt(level, scanX, scanY + height, scanZ - width))
                    return false;
            }
        }

        const BlockState portal = makePortalState("z");
        for (int32_t height = 0; height < innerHeight; ++height) {
            for (int32_t width = 0; width < innerWidth; ++width) {
                writeBlock(level, Vector3i(scanX, scanY + height, scanZ - width), portal, owner);
            }
        }

        if (owner != nullptr)
            owner->playLevelSound(level, LevelSoundEvent::FIRE_IGNITE, centerOf(position));

        return true;
    }

    return false;
}
