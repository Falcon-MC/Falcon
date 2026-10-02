#include "Block/Blocks/ObsidianBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockState.h"
#include "Block/Blocks/PortalBlock.h"
#include "Block/Blocks/PortalHelpers.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

FALCON_REGISTER_BLOCK(ObsidianBlock, 8);

using namespace PortalHelpers;

namespace {
    bool isObsidianAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return identifierAt(level, x, y, z) == OBSIDIAN_IDENTIFIER;
    }
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
