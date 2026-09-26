#include "Item/Items/ChorusFruitItem.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Blocks/LiquidBlock.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Item/ItemClassRegistry.h"
#include "Level/LevelChunk.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {
    const char *CHORUS_FRUIT_IDENTIFIER = "minecraft:chorus_fruit";
    const int TELEPORT_RANGE = 8;
    const int TELEPORT_ATTEMPTS = 128;

    std::mt19937 &randomGenerator() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

FALCON_REGISTER_ITEM(ChorusFruitItem, 100);

ChorusFruitItem::ChorusFruitItem(const Item &base) : Item(base) {
}

bool ChorusFruitItem::matches(const std::string &identifier) {
    return identifier == CHORUS_FRUIT_IDENTIFIER;
}

bool ChorusFruitItem::isChorusFruit(const ItemStack &item) {
    return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == CHORUS_FRUIT_IDENTIFIER;
}

bool ChorusFruitItem::canConsume(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!isChorusFruit(player.getInventory().getItemInHand()))
        return false;

    return !LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition()).water;
}

bool ChorusFruitItem::isSolid(Level &level, int x, int y, int z) {
    if (y < level.getMinY() || y > level.getMaxY())
        return true;

    const BlockState state = level.getBlockState(x, y, z);
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data != nullptr && data->mSolid;
}

bool ChorusFruitItem::isLiquid(Level &level, int x, int y, int z) {
    if (y < level.getMinY() || y > level.getMaxY())
        return true;

    return LiquidBlock(level.getBlockState(x, y, z)).isLiquid();
}

bool ChorusFruitItem::findTeleportPosition(ServerNetworkHandler &owner, ServerPlayer &player,
                                            Vector3f &destination) {
    Level &level = owner.getLevelFor(player);
    const Vector3f origin = player.getPosition();
    const int minWorldY = level.getMinY();
    const int maxWorldY = level.getMaxY();
    const int minX = (int) std::floor(origin.x) - TELEPORT_RANGE;
    const int minY = std::max(minWorldY, (int) std::floor(origin.y) - TELEPORT_RANGE);
    const int minZ = (int) std::floor(origin.z) - TELEPORT_RANGE;
    const int maxX = minX + TELEPORT_RANGE * 2;
    const int maxY = std::min(maxWorldY - 2, (int) std::floor(origin.y) + TELEPORT_RANGE);
    const int maxZ = minZ + TELEPORT_RANGE * 2;

    if (minY > maxY)
        return false;

    std::uniform_int_distribution<int> xDistribution(minX, maxX);
    std::uniform_int_distribution<int> yDistribution(minY, maxY);
    std::uniform_int_distribution<int> zDistribution(minZ, maxZ);

    for (int attempt = 0; attempt < TELEPORT_ATTEMPTS; ++attempt) {
        const int x = xDistribution(randomGenerator());
        int y = yDistribution(randomGenerator());
        const int z = zDistribution(randomGenerator());

        while (y >= minWorldY && !isSolid(level, x, y + 1, z))
            --y;

        ++y;
        if (y < minWorldY || y + 2 > maxWorldY)
            continue;

        if (isSolid(level, x, y + 1, z) || isLiquid(level, x, y + 1, z)
            || isSolid(level, x, y + 2, z) || isLiquid(level, x, y + 2, z))
            continue;

        destination = Vector3f((float) x + 0.5f, (float) y + 1.0f, (float) z + 0.5f);
        return true;
    }

    return false;
}

bool ChorusFruitItem::onEaten(ServerNetworkHandler &owner, ServerPlayer &player) {
    Vector3f destination;
    if (!findTeleportPosition(owner, player, destination))
        return true;

    Level &level = owner.getLevelFor(player);
    owner.playLevelSound(level, LevelSoundEvent::TELEPORT, player.getPosition(), player.getIdentifier());
    player.teleport(owner, destination, MovePlayerTeleportationCause::ChorusFruit);
    owner.playLevelSound(level, LevelSoundEvent::TELEPORT, destination, player.getIdentifier());
    return true;
}
