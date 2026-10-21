#include "Actor/ActorClassRegistry.h"
#include "Actor/VanillaActorTable.h"
#include "Actor/Projectile/ProjectileActor.h"
#include "Block/BlockData.h"
#include "Block/BlockPaletteRegistry.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/IceCrystalBlock.h"
#include "Block/Blocks/IcicleBlock.h"
#include "Block/Blocks/DiscPressBlock.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Inventory/ItemStackNbt.h"
#include "Item/ItemData.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/ItemNetworkIdTable.h"
#include "Item/Items/ThrowableItem.h"
#include "Item/VanillaItems.h"
#include "Item/PotionEffects.h"
#include "Level/BlockStateUpgrades.h"
#include "Level/Generator/End/Tree/LegacyChorusTree.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Level/SubChunk.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState block(const char *name) {
        const Block *value = VanillaBlocks::fromIdentifier(name);
        require(value != nullptr, "missing block registration");
        return value->toBlockState();
    }

    void migration() {
        BlockDefinitionRegistry blocks;
        ItemDefinitionRegistry items;
        PacketCodecContext context(blocks, items);
        for (const char *name: {"minecraft:chorus_plant", "minecraft:fire", "minecraft:redstone_wire"}) {
            Tag old = Tag::ofCompound();
            if (std::string(name) == "minecraft:fire")
                old.putInt("age", 7);
            if (std::string(name) == "minecraft:redstone_wire")
                old.putInt("redstone_signal", 11);
            // 26.50 and 26.60 have the same serialized state version.
            const BlockStateData upgraded = BlockStateUpgrades::upgrade(BlockStateData(name, old, 18168865));
            const Tag &states = upgraded.getStates();
            const bool wire = std::string(name) == "minecraft:redstone_wire";
            require(states.contains(wire ? "redstone_north" : "minecraft:connection_north"), "missing migrated shape");
            require(!wire || states.getInt("redstone_signal") == 11, "migration changed power");
            require(std::string(name) != "minecraft:fire" || states.getInt("age") == 7, "migration changed age");
            require(BlockStateUpgrades::upgrade(upgraded).getStates() == states, "migration is not idempotent");

            auto definition = std::make_shared<BlockDefinition>(name, BlockStateHasher::hash(name, states), states);
            blocks.registerDefinition(definition);
            items.registerDefinition(std::make_shared<ItemDefinition>(name, 1, false, Tag::ofCompound()));
            Tag saved = Tag::ofCompound();
            saved.putString("Name", name);
            saved.putByte("Count", 3);
            saved.put("Block", BlockStateData(name, old, 18168865).toNbt());
            const ItemStack item = ItemStackNbt::read(saved, context);
            require(item.mCount == 3 && item.mBlockDefinition == definition, "saved block item did not migrate");

            SubChunk chunk(0);
            chunk.setBlock(2, 3, 4, BlockState(name, old));
            BinaryStream bytes;
            chunk.writePersistent(bytes);
            ReadOnlyBinaryStream input(bytes.getBuffer());
            SubChunk loaded(0);
            require(loaded.readPersistent(input), "chunk reload failed");
            require(loaded.getBlock(2, 3, 4).mStates == states, "saved chunk did not migrate");
        }
        Tag connected = block("minecraft:chorus_plant").mStates;
        connected.putByte("minecraft:connection_east", 1);
        require(BlockStateUpgrades::upgrade(BlockStateData("minecraft:chorus_plant", connected, 18168865))
                    .getStates().getBool("minecraft:connection_east"), "migration erased an existing connection");
        require(BlockStateUpgrades::upgrade(BlockStateData("minecraft:fire", Tag::ofCompound(), 0)).getStates()
                    .contains("minecraft:connection_up"), "old-version migration missed new states");
        require(BlockStateUpgrades::upgrade(BlockStateData("minecraft:fire", Tag::ofCompound(), 18168866)).getStates()
                    == Tag::ofCompound(), "migration rewrote a future state");
    }

    void gameplay() {
        require(findPotionId("minecraft:freezing") == 47, "missing freezing potion");
        require(findPotionId("long_freezing") == 48, "missing long freezing potion");
        require(getPotionEffects(47).empty() && getPotionEffects(48).empty(), "unverified freezing effect");
        for (const char *potion: {"minecraft:potion", "minecraft:splash_potion", "minecraft:lingering_potion"}) {
            const BrewingMixData *mix = CraftingRecipeTable::findBrewingMix(potion, 4, "minecraft:ice_ball", 0);
            require(mix != nullptr && mix->mOutputMeta == 47, "missing freezing brewing recipe");
        }
        require(ItemDataTable::find("minecraft:ice_ball")->mMaxStackSize == 16, "incorrect ice ball stack size");
        require(ItemNetworkIdTable::find("minecraft:frostbite_spawn_egg") != nullptr, "missing spawn egg data");
        bool frostbite = false;
        for (size_t i = 0; i < VanillaActorTable::getCount(); ++i)
            frostbite |= std::string(VanillaActorTable::getIdentifiers()[i]) == "minecraft:frostbite";
        require(frostbite, "frostbite is absent from actor identifiers");
        require(dynamic_cast<const ThrowableItem *>(VanillaItems::fromIdentifier("minecraft:ice_ball")) != nullptr,
                "ice ball is not throwable");
        auto projectile = ActorClassRegistry::create(1, "minecraft:ice_ball");
        const auto *iceBall = dynamic_cast<const IceBallActor *>(projectile.get());
        require(iceBall != nullptr && iceBall->isProjectile(), "ice ball projectile missing");
        require(iceBall->getGravity() == 0.03f && iceBall->getInertia() == 0.99f, "incorrect ice ball physics");
        require(dynamic_cast<const DiscPressBlock *>(VanillaBlocks::fromIdentifier("minecraft:disc_press")) != nullptr,
                "missing disc press class");

        Level level("gameplay-2660-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(8, 20, 8);
        level.setBlock(Vector3i(8, 19, 8), block("minecraft:stone"), false);
        const auto *icicle = dynamic_cast<const IcicleBlock *>(VanillaBlocks::fromIdentifier("minecraft:icicle"));
        const auto *crystal = dynamic_cast<const IceCrystalBlock *>(VanillaBlocks::fromIdentifier("minecraft:ice_crystal"));
        require(icicle != nullptr && crystal != nullptr, "missing ice block classes");
        require(icicle->canPlaceAt(level, position, 1) && !icicle->canPlaceAt(level, position, 2), "invalid icicle placement");
        require(crystal->canPlaceAt(level, position, 1), "crystal cannot attach to stone");
        for (int face = 0; face < 6; ++face) {
            const Vector3i support = PlacementOrientation::relativePosition(position, face ^ 1);
            level.setBlock(support, block("minecraft:stone"), false);
            require(crystal->canPlaceAt(level, position, face), "crystal rejected a solid face");
            level.setBlock(support, block("minecraft:air"), false);
            require(!crystal->canPlaceAt(level, position, face), "crystal accepted an empty face");
        }
        BlockState slab = block("minecraft:smooth_stone_slab");
        slab.mStates.putString("minecraft:vertical_half", "bottom");
        require(!PlacementHelpers::hasSolidFace(slab, 1), "bottom slab has a full top face");
        level.setBlock(Vector3i(8, 19, 8), block("minecraft:stone"), false);
        BlockState upright = block("minecraft:icicle");
        require(icicle->canSurvive(level, position, upright), "icicle lost its support");
        level.setBlock(Vector3i(8, 19, 8), block("minecraft:air"), false);
        require(!icicle->canSurvive(level, position, upright), "unsupported icicle survived");
        level.setBlock(Vector3i(8, 21, 8), block("minecraft:stone"), false);
        BlockState hanging = upright;
        hanging.mStates.putByte("hanging", 1);
        require(icicle->canSurvive(level, position, hanging), "ceiling icicle rejected its support");
        level.setBlock(position, hanging, false);
        require(icicle->canPlaceAt(level, Vector3i(8, 19, 8), 0), "aligned icicles cannot stack");
        require(!icicle->canPlaceAt(level, Vector3i(8, 19, 8), 1), "icicle accepted opposite support");
        level.setBlock(Vector3i(8, 21, 8), block("minecraft:air"), false);

        const Block *wire = VanillaBlocks::fromIdentifier("minecraft:redstone_wire");
        BlockState powered = block("minecraft:redstone_wire");
        powered.mStates.putInt("redstone_signal", 9);
        level.setBlock(position, powered, false);
        BlockState shape = level.getBlockState(8, 20, 8);
        require(shape.mStates.getString("redstone_north") == "side" && shape.mStates.getInt("redstone_signal") == 9,
                "isolated wire shape changed signal");
        level.setBlock(Vector3i(9, 20, 8), powered, false);
        shape = wire->updateShape(level, position, shape);
        require(shape.mStates.getString("redstone_north") == "none"
                && shape.mStates.getString("redstone_west") == "side", "wire straight line shape incorrect");
        level.setBlock(Vector3i(9, 20, 8), block("minecraft:stone"), false);
        level.setBlock(Vector3i(9, 21, 8), powered, false);
        require(wire->updateShape(level, position, shape).mStates.getString("redstone_east") == "up", "wire cannot climb");

        level.setBlock(Vector3i(7, 20, 8), block("minecraft:oak_planks"), false);
        level.setBlock(position, block("minecraft:fire"), false);
        require(level.getBlockState(8, 20, 8).mStates.getBool("minecraft:connection_west"), "wall fire missing connection");
        level.setBlock(Vector3i(8, 19, 8), block("minecraft:stone"), false);
        level.setBlock(position, block("minecraft:fire"), false);
        require(!level.getBlockState(8, 20, 8).mStates.getBool("minecraft:connection_west"), "ground fire kept wall shape");

        level.setBlock(Vector3i(15, 30, 8), block("minecraft:chorus_plant"), false);
        LevelChunk neighbour(1, 0);
        neighbour.setBlock(0, 30, 8, block("minecraft:chorus_plant"));
        level.insertChunk(std::move(neighbour));
        require(level.getBlockState(15, 30, 8).mStates.getBool("minecraft:connection_east")
                && level.getBlockState(16, 30, 8).mStates.getBool("minecraft:connection_west"), "chunk border shapes not refreshed");
        Tag edgeStates = block("minecraft:chorus_plant").mStates;
        edgeStates.putByte("minecraft:connection_north", 1);
        level.setBlock(Vector3i(8, 30, 0), BlockState("minecraft:chorus_plant", edgeStates), false);
        require(level.getBlockState(8, 30, 0).mStates.getBool("minecraft:connection_north"), "unloaded neighbour erased saved shape");
        level.insertChunk(LevelChunk(0, -1));
        require(!level.getBlockState(8, 30, 0).mStates.getBool("minecraft:connection_north"), "loaded empty neighbour kept stale connection");

        BlockManager manager(level);
        SimpleRandom random(1234);
        LegacyChorusTree tree;
        manager.setBlockStateAt(8, 39, 8, block("minecraft:end_stone"));
        tree.generate(manager, random, 8, 40, 8);
        require(manager.getBlockAt(8, 40, 8).mStates.getBool("minecraft:connection_down"), "generated chorus missing root connection");
    }
}

int main() {
    try {
        migration();
        gameplay();
        std::cout << "26.60 migration and gameplay regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
