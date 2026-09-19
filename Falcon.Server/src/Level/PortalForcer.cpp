#include "Level/PortalForcer.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockState.h"
#include "Core/Debug/BedrockLog.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    const char *AIR_IDENTIFIER = "minecraft:air";
    const char *OBSIDIAN_IDENTIFIER = "minecraft:obsidian";
    const char *PORTAL_IDENTIFIER = "minecraft:portal";
    const char *END_PORTAL_IDENTIFIER = "minecraft:end_portal";
    const char *END_PORTAL_FRAME_IDENTIFIER = "minecraft:end_portal_frame";
    const char *ENDER_EYE_IDENTIFIER = "minecraft:ender_eye";
    const char *BEDROCK_IDENTIFIER = "minecraft:bedrock";

    const int32_t FRAME_OFFSETS[6][3] = {
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

    bool isSolidIdentifier(const std::string &identifier) {
        const BlockData *data = BlockDataTable::find(identifier.c_str());
        return data != nullptr && data->mSolid;
    }

    bool isLavaIdentifier(const std::string &identifier) {
        return identifier == "minecraft:lava" || identifier == "minecraft:flowing_lava";
    }

    bool isWaterIdentifier(const std::string &identifier) {
        return identifier == "minecraft:water" || identifier == "minecraft:flowing_water";
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

    int64_t packPosition(const Vector3i &position) {
        return ((int64_t) (position.x & 0x3FFFFFF) << 38)
               | ((int64_t) (position.z & 0x3FFFFFF) << 12)
               | (int64_t) (position.y & 0xFFF);
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

    void removeConnected(Level &level, const Vector3i &origin, const char *identifier, ServerNetworkHandler *owner) {
        std::vector<Vector3i> pending;
        std::unordered_set<int64_t> visited;

        pending.push_back(origin);
        visited.insert(packPosition(origin));

        const BlockState air((std::string(AIR_IDENTIFIER)));
        int32_t removed = 0;

        while (!pending.empty() && removed < PortalForcer::MAX_PORTAL_BLOCKS) {
            const Vector3i current = pending.back();
            pending.pop_back();

            if (identifierAt(level, current.x, current.y, current.z) == identifier) {
                writeBlock(level, current, air, owner);
                ++removed;
            } else if (current != origin) {
                continue;
            }

            for (const auto &offset: FRAME_OFFSETS) {
                const Vector3i neighbour(current.x + offset[0], current.y + offset[1], current.z + offset[2]);

                if (!isInsideLevel(level, neighbour))
                    continue;

                if (identifierAt(level, neighbour.x, neighbour.y, neighbour.z) != identifier)
                    continue;

                if (visited.insert(packPosition(neighbour)).second)
                    pending.push_back(neighbour);
            }
        }
    }
}

bool PortalForcer::isNetherPortalBlock(Level &level, const Vector3i &position) {
    return identifierAt(level, position.x, position.y, position.z) == PORTAL_IDENTIFIER;
}

bool PortalForcer::isEndPortalBlock(Level &level, const Vector3i &position) {
    return identifierAt(level, position.x, position.y, position.z) == END_PORTAL_IDENTIFIER;
}

bool PortalForcer::tryLightPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
    if (level.getDimensionType() == DimensionType::TheEnd)
        return false;

    const int32_t targetX = position.x;
    const int32_t targetY = position.y;
    const int32_t targetZ = position.z;

    if (!isObsidianAt(level, targetX, targetY, targetZ))
        return false;

    for (int32_t i = 1; i < 4; ++i) {
        if (!isAirAt(level, targetX, targetY + i, targetZ))
            return false;
    }

    int32_t sizePosX = 0;
    int32_t sizeNegX = 0;
    int32_t sizePosZ = 0;
    int32_t sizeNegZ = 0;

    for (int32_t i = 1; i < MAX_PORTAL_SIZE; ++i) {
        if (!isObsidianAt(level, targetX + i, targetY, targetZ))
            break;

        ++sizePosX;
    }

    for (int32_t i = 1; i < MAX_PORTAL_SIZE; ++i) {
        if (!isObsidianAt(level, targetX - i, targetY, targetZ))
            break;

        ++sizeNegX;
    }

    for (int32_t i = 1; i < MAX_PORTAL_SIZE; ++i) {
        if (!isObsidianAt(level, targetX, targetY, targetZ + i))
            break;

        ++sizePosZ;
    }

    for (int32_t i = 1; i < MAX_PORTAL_SIZE; ++i) {
        if (!isObsidianAt(level, targetX, targetY, targetZ - i))
            break;

        ++sizeNegZ;
    }

    const int32_t sizeX = sizePosX + sizeNegX + 1;
    const int32_t sizeZ = sizePosZ + sizeNegZ + 1;

    if (sizeX >= 2 && sizeX <= MAX_PORTAL_SIZE) {
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
        for (int32_t i = 0; i < MAX_PORTAL_SIZE - 2; ++i) {
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
        for (int32_t i = 0; i < MAX_PORTAL_SIZE - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX, scanY + i, scanZ);

            if (identifier == AIR_IDENTIFIER) {
                ++innerHeight;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        if (!(innerWidth <= MAX_PORTAL_SIZE - 2 && innerWidth >= 2
              && innerHeight <= MAX_PORTAL_SIZE - 2 && innerHeight >= 3))
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

    if (sizeZ >= 2 && sizeZ <= MAX_PORTAL_SIZE) {
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
        for (int32_t i = 0; i < MAX_PORTAL_SIZE - 2; ++i) {
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
        for (int32_t i = 0; i < MAX_PORTAL_SIZE - 2; ++i) {
            const std::string identifier = identifierAt(level, scanX, scanY + i, scanZ);

            if (identifier == AIR_IDENTIFIER) {
                ++innerHeight;
                continue;
            }

            if (identifier == OBSIDIAN_IDENTIFIER)
                break;

            return false;
        }

        if (!(innerWidth <= MAX_PORTAL_SIZE - 2 && innerWidth >= 2
              && innerHeight <= MAX_PORTAL_SIZE - 2 && innerHeight >= 3))
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

void PortalForcer::onFrameBlockBroken(Level &level, const Vector3i &position, const std::string &brokenIdentifier,
                                      ServerNetworkHandler *owner) {
    removeConnected(level, position, PORTAL_IDENTIFIER, owner);

    if (brokenIdentifier == END_PORTAL_IDENTIFIER || brokenIdentifier == END_PORTAL_FRAME_IDENTIFIER)
        removeConnected(level, position, END_PORTAL_IDENTIFIER, owner);
}

void PortalForcer::spawnPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
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

void PortalForcer::spawnObsidianPlatform(Level &level, const Vector3i &position, ServerNetworkHandler *owner) {
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

bool PortalForcer::findNearestPortal(Level &level, const Vector3i &origin, Vector3i &out) {
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

bool PortalForcer::findNetherPortalDestination(Level &destination, const Vector3i &source, Vector3i &out) {
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

        if (!space || !isSolidIdentifier(ground))
            continue;

        if (isLavaIdentifier(ground))
            continue;

        if (toNether) {
            if (ground == BEDROCK_IDENTIFIER)
                continue;
        } else if (isWaterIdentifier(ground)) {
            continue;
        }

        y = i;
        break;
    }

    const int32_t clamped = std::max(destination.getMinY(), std::min(destination.getMaxY(), y));
    out = Vector3i(x, clamped + 1, z);
    return true;
}

bool PortalForcer::tryCompleteEndPortal(Level &level, const Vector3i &framePosition, ServerNetworkHandler *owner) {
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

bool PortalForcer::tryInsertEnderEye(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    PlayerInventory &inventory = player.getInventory();
    const ItemStack &held = inventory.getItemInHand();

    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    if (held.mDefinition->getIdentifier() != ENDER_EYE_IDENTIFIER)
        return false;

    Level &level = owner.getLevelFor(player);
    if (!isInsideLevel(level, position))
        return false;

    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (state.mName != END_PORTAL_FRAME_IDENTIFIER)
        return false;

    if (stateFlag(state, "end_portal_eye_bit") != 0)
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

    tryCompleteEndPortal(level, position, &owner);
    return true;
}

void PortalForcer::tickPlayer(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead() || player.isAwaitingDimensionAck())
        return;

    if (player.getPortalCooldown() > 0) {
        player.setPortalCooldown(player.getPortalCooldown() - 1);
        player.setPortalTicks(0);
        return;
    }

    Level &level = owner.getLevelFor(player);
    const Vector3f position = player.getPosition();

    const int32_t minX = (int32_t) std::floor(position.x - 0.3f);
    const int32_t maxX = (int32_t) std::floor(position.x + 0.3f);
    const int32_t minZ = (int32_t) std::floor(position.z - 0.3f);
    const int32_t maxZ = (int32_t) std::floor(position.z + 0.3f);
    const int32_t minY = (int32_t) std::floor(position.y);
    const int32_t maxY = (int32_t) std::floor(position.y + 1.8f);

    bool inNetherPortal = false;
    bool inEndPortal = false;

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    continue;

                if (!level.isChunkResident(x >> 4, z >> 4))
                    continue;

                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr)
                    continue;

                if (state->mName == PORTAL_IDENTIFIER)
                    inNetherPortal = true;
                else if (state->mName == END_PORTAL_IDENTIFIER)
                    inEndPortal = true;
            }
        }
    }

    if (inEndPortal) {
        player.setPortalTicks(0);

        const DimensionType current = level.getDimensionType();

        if (current != DimensionType::TheEnd) {
            Level &end = owner.getDimension(DimensionType::TheEnd);
            const Vector3i platform(END_PLATFORM_X, END_PLATFORM_Y, END_PLATFORM_Z);
            spawnObsidianPlatform(end, platform, &owner);

            player.setPortalCooldown(PORTAL_COOLDOWN_TICKS);
            owner.changePlayerDimension(player, DimensionType::TheEnd,
                                        Vector3f((float) END_PLATFORM_X + 0.5f,
                                                 (float) END_PLATFORM_Y + 1.0f,
                                                 (float) END_PLATFORM_Z + 0.5f));
            return;
        }

        if (current == DimensionType::TheEnd) {
            Level &overworld = owner.getDimension(DimensionType::Overworld);

            player.setPortalCooldown(PORTAL_COOLDOWN_TICKS);
            owner.changePlayerDimension(player, DimensionType::Overworld,
                                        overworld.getSpawnPositionForPlayer());
        }

        return;
    }

    if (!inNetherPortal) {
        player.setPortalTicks(0);
        return;
    }

    const int32_t ticks = player.getPortalTicks() + 1;
    player.setPortalTicks(ticks);

    const int32_t requiredTicks = player.getGameType() == (int32_t) GameType::Creative
                                  ? 1
                                  : PORTAL_DELAY_TICKS;

    if (ticks < requiredTicks)
        return;

    player.setPortalTicks(0);

    const DimensionType current = level.getDimensionType();
    if (current != DimensionType::Overworld && current != DimensionType::Nether)
        return;

    const DimensionType target = current == DimensionType::Overworld
                                 ? DimensionType::Nether
                                 : DimensionType::Overworld;

    Level &destination = owner.getDimension(target);

    const Vector3i source((int32_t) std::floor(position.x),
                          (int32_t) std::floor(position.y),
                          (int32_t) std::floor(position.z));

    Vector3i landing(0, 0, 0);
    if (!findNetherPortalDestination(destination, source, landing))
        return;

    Vector3i existing(0, 0, 0);
    Vector3f arrival(0.0f, 0.0f, 0.0f);

    if (findNearestPortal(destination, landing, existing)) {
        arrival = Vector3f((float) existing.x + 0.5f, (float) existing.y, (float) existing.z + 0.5f);
    } else {
        spawnPortal(destination, landing, &owner);
        arrival = Vector3f((float) landing.x + 1.5f, (float) landing.y + 1.0f, (float) landing.z + 1.5f);
    }

    player.setPortalCooldown(PORTAL_COOLDOWN_TICKS);
    owner.changePlayerDimension(player, target, arrival);
}
