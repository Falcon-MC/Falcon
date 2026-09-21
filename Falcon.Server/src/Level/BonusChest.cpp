#include "Level/BonusChest.h"

#include "Block/Actor/ChestBlockActor.h"
#include "Block/BlockPaletteRegistry.h"
#include "Level/Level.h"
#include "Loot/LootItems.h"
#include "Loot/LootTableRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"

#include <cstdlib>
#include <random>

namespace {
    const char *BONUS_CHEST_LOOT_TABLE = "chests/spawn_bonus_chest.json";
    const int32_t SEARCH_RADIUS = 3;

    BlockState defaultState(const std::string &identifier) {
        BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        registry.initialize();

        const Tag *states = registry.getDefaultStates(identifier);
        return states == nullptr ? BlockState(identifier) : BlockState(identifier, *states);
    }

    bool isFreeAbove(Level &level, int32_t x, int32_t y, int32_t z) {
        return level.getBlockState(x, y, z).mName == "minecraft:air" && level.isSolidAt(x, y - 1, z);
    }
}

bool BonusChest::placeIfPending(ServerNetworkHandler &owner, Level &level) {
    if (!level.isBonusChestPending())
        return false;

    Vector3i position;
    if (!_findPosition(level, level.findSafeSpawn(level.getSpawnPosition()), position))
        return false;

    level.setBlock(position, defaultState("minecraft:chest"), true);
    _fill(owner, level, position);

    const BlockState torch = defaultState("minecraft:torch");
    const Vector3i sides[] = {Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1)};
    for (const Vector3i &side: sides) {
        const Vector3i target(position.x + side.x, position.y, position.z + side.z);
        if (isFreeAbove(level, target.x, target.y, target.z))
            level.setBlock(target, torch, true);
    }

    level.markBonusChestSpawned();
    level.saveLevelDat();
    return true;
}

bool BonusChest::_findPosition(Level &level, const Vector3i &around, Vector3i &out) {
    for (int32_t radius = 1; radius <= SEARCH_RADIUS; ++radius) {
        for (int32_t dx = -radius; dx <= radius; ++dx) {
            for (int32_t dz = -radius; dz <= radius; ++dz) {
                if (std::abs(dx) != radius && std::abs(dz) != radius)
                    continue;

                const int32_t x = around.x + dx;
                const int32_t z = around.z + dz;
                for (int32_t dy = 2; dy >= -2; --dy) {
                    const int32_t y = around.y + dy;
                    if (isFreeAbove(level, x, y, z)) {
                        out = Vector3i(x, y, z);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void BonusChest::_fill(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    static std::mt19937 random(std::random_device{}());

    const LootTable *table = LootTableRegistry::getInstance().get(BONUS_CHEST_LOOT_TABLE);
    if (table == nullptr)
        return;

    LootContext context(random);
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mRegionalDifficulty = level.getRegionalDifficulty(context.mDifficulty);
    ChestBlockActor &chest = level.getBlockActors().getOrCreate<ChestBlockActor>(position);
    LootItems::fillContainer(owner, chest.getInventory(), table->roll(context), random);
}
