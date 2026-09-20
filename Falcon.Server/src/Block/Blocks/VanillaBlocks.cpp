#include "Block/Blocks/VanillaBlocks.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockPaletteRegistry.h"
#include "Block/BlockTypeIds.h"
#include "Core/Debug/BedrockLog.h"

#include <string>
#include <unordered_set>

#include <string_view>
#include <unordered_map>

namespace {
    Block buildFromTypeId(int32_t typeId) {
        const BlockData *data = BlockDataTable::findByTypeId(typeId);
        if (data == nullptr)
            return Block(BlockTypeIds::AIR, "minecraft:air", "Air");

        return VanillaBlocks::fromData(*data);
    }
}

namespace {
    std::unique_ptr<Block> makeBlock(const BlockData &data) {
        return BlockClassRegistry::create(VanillaBlocks::fromData(data));
    }
}

Block VanillaBlocks::fromData(const BlockData &data) {
    const std::string identifier = data.mIdentifier;
    BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
    registry.initialize();
    const Tag *defaultStates = registry.getDefaultStates(identifier);

    if (defaultStates != nullptr)
        return Block(data.mTypeId, data.mIdentifier, data.mName, *defaultStates);

    static std::unordered_set<std::string> warnedIdentifiers;
    if (warnedIdentifiers.insert(identifier).second)
        LOG_WARN(LogAreaID::Server, "Block palette has no default states for %s", identifier.c_str());

    return Block(data.mTypeId, data.mIdentifier, data.mName, Tag::ofCompound());
}

Block VanillaBlocks::CYAN_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::CYAN_TERRACOTTA);
}

Block VanillaBlocks::HARD_PINK_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_PINK_STAINED_GLASS);
}

Block VanillaBlocks::BLUE_CANDLE() {
    return buildFromTypeId(BlockTypeIds::BLUE_CANDLE);
}

Block VanillaBlocks::DARK_OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_WOOD);
}

Block VanillaBlocks::BIRCH_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::BIRCH_STANDING_SIGN);
}

Block VanillaBlocks::POLISHED_BASALT() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BASALT);
}

Block VanillaBlocks::NETHER_GOLD_ORE() {
    return buildFromTypeId(BlockTypeIds::NETHER_GOLD_ORE);
}

Block VanillaBlocks::ZOMBIE_HEAD() {
    return buildFromTypeId(BlockTypeIds::ZOMBIE_HEAD);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_CHAIN);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_CHEST);
}

Block VanillaBlocks::LEAF_LITTER() {
    return buildFromTypeId(BlockTypeIds::LEAF_LITTER);
}

Block VanillaBlocks::WARPED_DOOR() {
    return buildFromTypeId(BlockTypeIds::WARPED_DOOR);
}

Block VanillaBlocks::LIGHT_BLUE_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CONCRETE_POWDER);
}

Block VanillaBlocks::BAMBOO_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_BLOCK);
}

Block VanillaBlocks::PISTON_ARM_COLLISION() {
    return buildFromTypeId(BlockTypeIds::PISTON_ARM_COLLISION);
}

Block VanillaBlocks::WAXED_OXIDIZED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_CHISELED_COPPER);
}

Block VanillaBlocks::WET_SPONGE() {
    return buildFromTypeId(BlockTypeIds::WET_SPONGE);
}

Block VanillaBlocks::END_STONE_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::END_STONE_BRICK_WALL);
}

Block VanillaBlocks::GRANITE() {
    return buildFromTypeId(BlockTypeIds::GRANITE);
}

Block VanillaBlocks::BLUE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::BLUE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::FENCE_GATE);
}

Block VanillaBlocks::BIRCH_SHELF() {
    return buildFromTypeId(BlockTypeIds::BIRCH_SHELF);
}

Block VanillaBlocks::POWDER_SNOW() {
    return buildFromTypeId(BlockTypeIds::POWDER_SNOW);
}

Block VanillaBlocks::DARK_OAK_BUTTON() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_BUTTON);
}

Block VanillaBlocks::DEEPSLATE_COPPER_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_COPPER_ORE);
}

Block VanillaBlocks::CHISELED_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CHISELED_STONE_BRICKS);
}

Block VanillaBlocks::NETHER_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK_STAIRS);
}

Block VanillaBlocks::YELLOW_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::YELLOW_SHULKER_BOX);
}

Block VanillaBlocks::BLACKSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACKSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIME_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::LIME_STAINED_GLASS);
}

Block VanillaBlocks::RED_WOOL() {
    return buildFromTypeId(BlockTypeIds::RED_WOOL);
}

Block VanillaBlocks::POPLAR_BUTTON() {
    return buildFromTypeId(BlockTypeIds::POPLAR_BUTTON);
}

Block VanillaBlocks::JUNGLE_BUTTON() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_BUTTON);
}

Block VanillaBlocks::SPRUCE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_STAIRS);
}

Block VanillaBlocks::HARD_GREEN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_GREEN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::ACACIA_SHELF() {
    return buildFromTypeId(BlockTypeIds::ACACIA_SHELF);
}

Block VanillaBlocks::DIORITE() {
    return buildFromTypeId(BlockTypeIds::DIORITE);
}

Block VanillaBlocks::PALE_OAK_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_FENCE_GATE);
}

Block VanillaBlocks::GRAY_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::GRAY_CANDLE_CAKE);
}

Block VanillaBlocks::POLISHED_TUFF_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_TUFF_SLAB);
}

Block VanillaBlocks::CHERRY_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::CHERRY_PRESSURE_PLATE);
}

Block VanillaBlocks::CHERRY_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::CHERRY_HANGING_SIGN);
}

Block VanillaBlocks::YELLOW_WOOL() {
    return buildFromTypeId(BlockTypeIds::YELLOW_WOOL);
}

Block VanillaBlocks::CRIMSON_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_WALL_SIGN);
}

Block VanillaBlocks::YELLOW_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::YELLOW_STAINED_GLASS_PANE);
}

Block VanillaBlocks::END_GATEWAY() {
    return buildFromTypeId(BlockTypeIds::END_GATEWAY);
}

Block VanillaBlocks::AZURE_BLUET() {
    return buildFromTypeId(BlockTypeIds::AZURE_BLUET);
}

Block VanillaBlocks::BEACON() {
    return buildFromTypeId(BlockTypeIds::BEACON);
}

Block VanillaBlocks::RED_NETHER_BRICK() {
    return buildFromTypeId(BlockTypeIds::RED_NETHER_BRICK);
}

Block VanillaBlocks::BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::BRICK_WALL);
}

Block VanillaBlocks::POLISHED_SULFUR() {
    return buildFromTypeId(BlockTypeIds::POLISHED_SULFUR);
}

Block VanillaBlocks::COBBLED_DEEPSLATE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::COBBLED_DEEPSLATE_STAIRS);
}

Block VanillaBlocks::SMOOTH_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_SANDSTONE);
}

Block VanillaBlocks::SNOW_LAYER() {
    return buildFromTypeId(BlockTypeIds::SNOW_LAYER);
}

Block VanillaBlocks::BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::BLACK_CANDLE() {
    return buildFromTypeId(BlockTypeIds::BLACK_CANDLE);
}

Block VanillaBlocks::BLUE_CARPET() {
    return buildFromTypeId(BlockTypeIds::BLUE_CARPET);
}

Block VanillaBlocks::GLOW_FRAME() {
    return buildFromTypeId(BlockTypeIds::GLOW_FRAME);
}

Block VanillaBlocks::MUD_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MUD_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::HANGING_ROOTS() {
    return buildFromTypeId(BlockTypeIds::HANGING_ROOTS);
}

Block VanillaBlocks::RED_SANDSTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::RED_SANDSTONE_WALL);
}

Block VanillaBlocks::PRISMARINE_BRICKS_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_BRICKS_STAIRS);
}

Block VanillaBlocks::WAXED_OXIDIZED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_CUT_COPPER);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_CHAIN);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_CHEST);
}

Block VanillaBlocks::CALCITE() {
    return buildFromTypeId(BlockTypeIds::CALCITE);
}

Block VanillaBlocks::DIORITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DIORITE_SLAB);
}

Block VanillaBlocks::STRIPPED_DARK_OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_DARK_OAK_LOG);
}

Block VanillaBlocks::HARD_ORANGE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_ORANGE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::DEAD_BUBBLE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_BUBBLE_CORAL_FAN);
}

Block VanillaBlocks::JUNGLE_LOG() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_LOG);
}

Block VanillaBlocks::BUBBLE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::BUBBLE_CORAL_FAN);
}

Block VanillaBlocks::HARD_BROWN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_BROWN_STAINED_GLASS);
}

Block VanillaBlocks::SCULK_SHRIEKER() {
    return buildFromTypeId(BlockTypeIds::SCULK_SHRIEKER);
}

Block VanillaBlocks::GRAY_WOOL() {
    return buildFromTypeId(BlockTypeIds::GRAY_WOOL);
}

Block VanillaBlocks::ORANGE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::ORANGE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::HARD_BLACK_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_BLACK_STAINED_GLASS_PANE);
}

Block VanillaBlocks::GRAY_CARPET() {
    return buildFromTypeId(BlockTypeIds::GRAY_CARPET);
}

Block VanillaBlocks::LILY_OF_THE_VALLEY() {
    return buildFromTypeId(BlockTypeIds::LILY_OF_THE_VALLEY);
}

Block VanillaBlocks::LIME_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::LIME_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::TRAPDOOR);
}

Block VanillaBlocks::CACTUS_FLOWER() {
    return buildFromTypeId(BlockTypeIds::CACTUS_FLOWER);
}

Block VanillaBlocks::DEAD_BRAIN_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_BRAIN_CORAL_FAN);
}

Block VanillaBlocks::INFO_UPDATE() {
    return buildFromTypeId(BlockTypeIds::INFO_UPDATE);
}

Block VanillaBlocks::SEAGRASS() {
    return buildFromTypeId(BlockTypeIds::SEAGRASS);
}

Block VanillaBlocks::TUBE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::TUBE_CORAL_FAN);
}

Block VanillaBlocks::WAXED_EXPOSED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::REDSTONE_LAMP() {
    return buildFromTypeId(BlockTypeIds::REDSTONE_LAMP);
}

Block VanillaBlocks::MOSSY_COBBLESTONE() {
    return buildFromTypeId(BlockTypeIds::MOSSY_COBBLESTONE);
}

Block VanillaBlocks::DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE);
}

Block VanillaBlocks::POPLAR_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::POPLAR_TRAPDOOR);
}

Block VanillaBlocks::MAGENTA_CARPET() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CARPET);
}

Block VanillaBlocks::PITCHER_CROP() {
    return buildFromTypeId(BlockTypeIds::PITCHER_CROP);
}

Block VanillaBlocks::BROWN_WOOL() {
    return buildFromTypeId(BlockTypeIds::BROWN_WOOL);
}

Block VanillaBlocks::WAXED_EXPOSED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_CHISELED_COPPER);
}

Block VanillaBlocks::TUFF_SLAB() {
    return buildFromTypeId(BlockTypeIds::TUFF_SLAB);
}

Block VanillaBlocks::CINNABAR_WALL() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_WALL);
}

Block VanillaBlocks::WARPED_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::WARPED_PRESSURE_PLATE);
}

Block VanillaBlocks::STRIPPED_ACACIA_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_ACACIA_WOOD);
}

Block VanillaBlocks::FIREFLY_BUSH() {
    return buildFromTypeId(BlockTypeIds::FIREFLY_BUSH);
}

Block VanillaBlocks::DIAMOND_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DIAMOND_BLOCK);
}

Block VanillaBlocks::DARK_PRISMARINE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DARK_PRISMARINE_DOUBLE_SLAB);
}

Block VanillaBlocks::OAK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::OAK_STAIRS);
}

Block VanillaBlocks::HARD_GRAY_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_GRAY_STAINED_GLASS);
}

Block VanillaBlocks::OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::OAK_LOG);
}

Block VanillaBlocks::BROWN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::BROWN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::SULFUR_SPIKE() {
    return buildFromTypeId(BlockTypeIds::SULFUR_SPIKE);
}

Block VanillaBlocks::END_BRICKS() {
    return buildFromTypeId(BlockTypeIds::END_BRICKS);
}

Block VanillaBlocks::MAGENTA_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_SHULKER_BOX);
}

Block VanillaBlocks::PACKED_ICE() {
    return buildFromTypeId(BlockTypeIds::PACKED_ICE);
}

Block VanillaBlocks::PACKED_MUD() {
    return buildFromTypeId(BlockTypeIds::PACKED_MUD);
}

Block VanillaBlocks::LIGHT_BLUE_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CANDLE_CAKE);
}

Block VanillaBlocks::MOSS_CARPET() {
    return buildFromTypeId(BlockTypeIds::MOSS_CARPET);
}

Block VanillaBlocks::WARPED_FUNGUS() {
    return buildFromTypeId(BlockTypeIds::WARPED_FUNGUS);
}

Block VanillaBlocks::OXIDIZED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_LIGHTNING_ROD);
}

Block VanillaBlocks::POLISHED_DEEPSLATE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DEEPSLATE_SLAB);
}

Block VanillaBlocks::BAMBOO_DOOR() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_DOOR);
}

Block VanillaBlocks::AMETHYST_BLOCK() {
    return buildFromTypeId(BlockTypeIds::AMETHYST_BLOCK);
}

Block VanillaBlocks::DEAD_BUBBLE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_BUBBLE_CORAL_WALL_FAN);
}

Block VanillaBlocks::GOLD_BLOCK() {
    return buildFromTypeId(BlockTypeIds::GOLD_BLOCK);
}

Block VanillaBlocks::FLOWER_POT() {
    return buildFromTypeId(BlockTypeIds::FLOWER_POT);
}

Block VanillaBlocks::CHISELED_BOOKSHELF() {
    return buildFromTypeId(BlockTypeIds::CHISELED_BOOKSHELF);
}

Block VanillaBlocks::POLISHED_DEEPSLATE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DEEPSLATE_STAIRS);
}

Block VanillaBlocks::LIME_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::LIME_SHULKER_BOX);
}

Block VanillaBlocks::WEATHERED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_CHISELED_COPPER);
}

Block VanillaBlocks::SMALL_AMETHYST_BUD() {
    return buildFromTypeId(BlockTypeIds::SMALL_AMETHYST_BUD);
}

Block VanillaBlocks::GOLDEN_DANDELION() {
    return buildFromTypeId(BlockTypeIds::GOLDEN_DANDELION);
}

Block VanillaBlocks::ACTIVATOR_RAIL() {
    return buildFromTypeId(BlockTypeIds::ACTIVATOR_RAIL);
}

Block VanillaBlocks::IRON_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::IRON_TRAPDOOR);
}

Block VanillaBlocks::POTATOES() {
    return buildFromTypeId(BlockTypeIds::POTATOES);
}

Block VanillaBlocks::MUDDY_MANGROVE_ROOTS() {
    return buildFromTypeId(BlockTypeIds::MUDDY_MANGROVE_ROOTS);
}

Block VanillaBlocks::PALE_OAK_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_PRESSURE_PLATE);
}

Block VanillaBlocks::STRIPPED_JUNGLE_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_JUNGLE_WOOD);
}

Block VanillaBlocks::NOTEBLOCK() {
    return buildFromTypeId(BlockTypeIds::NOTEBLOCK);
}

Block VanillaBlocks::TUFF() {
    return buildFromTypeId(BlockTypeIds::TUFF);
}

Block VanillaBlocks::MANGROVE_LOG() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_LOG);
}

Block VanillaBlocks::OXIDIZED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::PALE_OAK_FENCE() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_FENCE);
}

Block VanillaBlocks::PALE_OAK_LEAVES() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_LEAVES);
}

Block VanillaBlocks::DEEPSLATE_TILE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_TILE_DOUBLE_SLAB);
}

Block VanillaBlocks::SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SANDSTONE_SLAB);
}

Block VanillaBlocks::MOSSY_STONE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::MOSSY_STONE_BRICK_SLAB);
}

Block VanillaBlocks::RAW_GOLD_BLOCK() {
    return buildFromTypeId(BlockTypeIds::RAW_GOLD_BLOCK);
}

Block VanillaBlocks::ALLIUM() {
    return buildFromTypeId(BlockTypeIds::ALLIUM);
}

Block VanillaBlocks::WHITE_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::WHITE_SHULKER_BOX);
}

Block VanillaBlocks::COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::COPPER_GRATE);
}

Block VanillaBlocks::BLACK_WOOL() {
    return buildFromTypeId(BlockTypeIds::BLACK_WOOL);
}

Block VanillaBlocks::ORANGE_CANDLE() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CANDLE);
}

Block VanillaBlocks::POWERED_COMPARATOR() {
    return buildFromTypeId(BlockTypeIds::POWERED_COMPARATOR);
}

Block VanillaBlocks::JUNGLE_FENCE() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_FENCE);
}

Block VanillaBlocks::CUT_SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CUT_SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::WARPED_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::WARPED_WALL_SIGN);
}

Block VanillaBlocks::SPRUCE_FENCE() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_FENCE);
}

Block VanillaBlocks::DARK_OAK_SAPLING() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_SAPLING);
}

Block VanillaBlocks::MELON_BLOCK() {
    return buildFromTypeId(BlockTypeIds::MELON_BLOCK);
}

Block VanillaBlocks::BLACK_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::BLACK_CONCRETE_POWDER);
}

Block VanillaBlocks::SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::WAXED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WAXED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::OPEN_EYEBLOSSOM() {
    return buildFromTypeId(BlockTypeIds::OPEN_EYEBLOSSOM);
}

Block VanillaBlocks::MOB_SPAWNER() {
    return buildFromTypeId(BlockTypeIds::MOB_SPAWNER);
}

Block VanillaBlocks::PALE_OAK_SAPLING() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_SAPLING);
}

Block VanillaBlocks::POLISHED_GRANITE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_GRANITE);
}

Block VanillaBlocks::PALE_OAK_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_WALL_SIGN);
}

Block VanillaBlocks::POPLAR_SLAB() {
    return buildFromTypeId(BlockTypeIds::POPLAR_SLAB);
}

Block VanillaBlocks::SOUL_FIRE() {
    return buildFromTypeId(BlockTypeIds::SOUL_FIRE);
}

Block VanillaBlocks::MAGENTA_CANDLE() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CANDLE);
}

Block VanillaBlocks::MANGROVE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_DOUBLE_SLAB);
}

Block VanillaBlocks::SMOOTH_QUARTZ_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_QUARTZ_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_STAINED_GLASS);
}

Block VanillaBlocks::OBSIDIAN() {
    return buildFromTypeId(BlockTypeIds::OBSIDIAN);
}

Block VanillaBlocks::LIGHT_GRAY_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_STAINED_GLASS_PANE);
}

Block VanillaBlocks::DARK_OAK_SLAB() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_SLAB);
}

Block VanillaBlocks::DEEPSLATE_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_BRICK_WALL);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_GRATE);
}

Block VanillaBlocks::OXIDIZED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::EXPOSED_COPPER() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER);
}

Block VanillaBlocks::POLISHED_DEEPSLATE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DEEPSLATE_DOUBLE_SLAB);
}

Block VanillaBlocks::WAXED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_BARS);
}

Block VanillaBlocks::STONE_BUTTON() {
    return buildFromTypeId(BlockTypeIds::STONE_BUTTON);
}

Block VanillaBlocks::RED_NETHER_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_NETHER_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::WAXED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_BULB);
}

Block VanillaBlocks::SPONGE() {
    return buildFromTypeId(BlockTypeIds::SPONGE);
}

Block VanillaBlocks::EXPOSED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::BAMBOO_FENCE() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_FENCE);
}

Block VanillaBlocks::NORMAL_STONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::NORMAL_STONE_STAIRS);
}

Block VanillaBlocks::DIORITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DIORITE_DOUBLE_SLAB);
}

Block VanillaBlocks::END_STONE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::END_STONE_BRICK_SLAB);
}

Block VanillaBlocks::HARDENED_CLAY() {
    return buildFromTypeId(BlockTypeIds::HARDENED_CLAY);
}

Block VanillaBlocks::BIRCH_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::BIRCH_HANGING_SIGN);
}

Block VanillaBlocks::STRIPPED_JUNGLE_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_JUNGLE_LOG);
}

Block VanillaBlocks::OXIDIZED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::LIGHT_BLOCK_9() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_9);
}

Block VanillaBlocks::LIGHT_BLOCK_8() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_8);
}

Block VanillaBlocks::LIGHT_BLOCK_7() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_7);
}

Block VanillaBlocks::LIGHT_BLOCK_6() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_6);
}

Block VanillaBlocks::LIGHT_BLOCK_5() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_5);
}

Block VanillaBlocks::LIGHT_BLOCK_4() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_4);
}

Block VanillaBlocks::LIGHT_BLOCK_3() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_3);
}

Block VanillaBlocks::LIGHT_BLOCK_2() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_2);
}

Block VanillaBlocks::LIGHT_BLOCK_1() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_1);
}

Block VanillaBlocks::LIGHT_BLOCK_0() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_0);
}

Block VanillaBlocks::PALE_OAK_DOOR() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_DOOR);
}

Block VanillaBlocks::OAK_SAPLING() {
    return buildFromTypeId(BlockTypeIds::OAK_SAPLING);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_TERRACOTTA);
}

Block VanillaBlocks::SMOKER() {
    return buildFromTypeId(BlockTypeIds::SMOKER);
}

Block VanillaBlocks::BROWN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::BROWN_STAINED_GLASS);
}

Block VanillaBlocks::ANDESITE() {
    return buildFromTypeId(BlockTypeIds::ANDESITE);
}

Block VanillaBlocks::FIRE_CORAL() {
    return buildFromTypeId(BlockTypeIds::FIRE_CORAL);
}

Block VanillaBlocks::STONE() {
    return buildFromTypeId(BlockTypeIds::STONE);
}

Block VanillaBlocks::SMOOTH_SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_SANDSTONE_SLAB);
}

Block VanillaBlocks::BIRCH_LOG() {
    return buildFromTypeId(BlockTypeIds::BIRCH_LOG);
}

Block VanillaBlocks::HARD_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_GLASS_PANE);
}

Block VanillaBlocks::TUFF_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::TUFF_BRICK_WALL);
}

Block VanillaBlocks::PURPUR_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPUR_SLAB);
}

Block VanillaBlocks::BRAIN_CORAL() {
    return buildFromTypeId(BlockTypeIds::BRAIN_CORAL);
}

Block VanillaBlocks::STRIPPED_SPRUCE_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_SPRUCE_WOOD);
}

Block VanillaBlocks::ORANGE_WOOL() {
    return buildFromTypeId(BlockTypeIds::ORANGE_WOOL);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::CRIMSON_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_DOUBLE_SLAB);
}

Block VanillaBlocks::RESPAWN_ANCHOR() {
    return buildFromTypeId(BlockTypeIds::RESPAWN_ANCHOR);
}

Block VanillaBlocks::LIGHT_GRAY_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CONCRETE);
}

Block VanillaBlocks::GREEN_CANDLE() {
    return buildFromTypeId(BlockTypeIds::GREEN_CANDLE);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER);
}

Block VanillaBlocks::RED_SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::BIRCH_WOOD() {
    return buildFromTypeId(BlockTypeIds::BIRCH_WOOD);
}

Block VanillaBlocks::RED_SAND() {
    return buildFromTypeId(BlockTypeIds::RED_SAND);
}

Block VanillaBlocks::HAY_BLOCK() {
    return buildFromTypeId(BlockTypeIds::HAY_BLOCK);
}

Block VanillaBlocks::JUNGLE_WOOD() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_WOOD);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER);
}

Block VanillaBlocks::INFESTED_CRACKED_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::INFESTED_CRACKED_STONE_BRICKS);
}

Block VanillaBlocks::WAXED_OXIDIZED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::OAK_LEAVES() {
    return buildFromTypeId(BlockTypeIds::OAK_LEAVES);
}

Block VanillaBlocks::RESIN_CLUMP() {
    return buildFromTypeId(BlockTypeIds::RESIN_CLUMP);
}

Block VanillaBlocks::BRAIN_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::BRAIN_CORAL_FAN);
}

Block VanillaBlocks::CYAN_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::CYAN_CANDLE_CAKE);
}

Block VanillaBlocks::POLISHED_TUFF_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_TUFF_WALL);
}

Block VanillaBlocks::BAMBOO_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_STAIRS);
}

Block VanillaBlocks::INFESTED_MOSSY_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::INFESTED_MOSSY_STONE_BRICKS);
}

Block VanillaBlocks::TORCH() {
    return buildFromTypeId(BlockTypeIds::TORCH);
}

Block VanillaBlocks::SULFUR_BRICKS() {
    return buildFromTypeId(BlockTypeIds::SULFUR_BRICKS);
}

Block VanillaBlocks::MUD_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::MUD_BRICK_WALL);
}

Block VanillaBlocks::HONEY_BLOCK() {
    return buildFromTypeId(BlockTypeIds::HONEY_BLOCK);
}

Block VanillaBlocks::UNDERWATER_TNT() {
    return buildFromTypeId(BlockTypeIds::UNDERWATER_TNT);
}

Block VanillaBlocks::DRIPSTONE_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DRIPSTONE_BLOCK);
}

Block VanillaBlocks::VINE() {
    return buildFromTypeId(BlockTypeIds::VINE);
}

Block VanillaBlocks::RED_SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_SANDSTONE_SLAB);
}

Block VanillaBlocks::CHERRY_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::CHERRY_TRAPDOOR);
}

Block VanillaBlocks::BLACKSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACKSTONE_SLAB);
}

Block VanillaBlocks::GOLD_ORE() {
    return buildFromTypeId(BlockTypeIds::GOLD_ORE);
}

Block VanillaBlocks::YELLOW_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::YELLOW_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::STONECUTTER() {
    return buildFromTypeId(BlockTypeIds::STONECUTTER);
}

Block VanillaBlocks::POPLAR_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::POPLAR_STANDING_SIGN);
}

Block VanillaBlocks::DRIED_GHAST() {
    return buildFromTypeId(BlockTypeIds::DRIED_GHAST);
}

Block VanillaBlocks::WARPED_PLANKS() {
    return buildFromTypeId(BlockTypeIds::WARPED_PLANKS);
}

Block VanillaBlocks::PISTON() {
    return buildFromTypeId(BlockTypeIds::PISTON);
}

Block VanillaBlocks::BROWN_CARPET() {
    return buildFromTypeId(BlockTypeIds::BROWN_CARPET);
}

Block VanillaBlocks::STONE_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::STONE_BRICK_STAIRS);
}

Block VanillaBlocks::DEAD_BUBBLE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DEAD_BUBBLE_CORAL_BLOCK);
}

Block VanillaBlocks::GRAY_CANDLE() {
    return buildFromTypeId(BlockTypeIds::GRAY_CANDLE);
}

Block VanillaBlocks::CHERRY_FENCE() {
    return buildFromTypeId(BlockTypeIds::CHERRY_FENCE);
}

Block VanillaBlocks::MANGROVE_PLANKS() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_PLANKS);
}

Block VanillaBlocks::INVISIBLE_BEDROCK() {
    return buildFromTypeId(BlockTypeIds::INVISIBLE_BEDROCK);
}

Block VanillaBlocks::RED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::RED_TERRACOTTA);
}

Block VanillaBlocks::DIORITE_WALL() {
    return buildFromTypeId(BlockTypeIds::DIORITE_WALL);
}

Block VanillaBlocks::DEAD_FIRE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DEAD_FIRE_CORAL_BLOCK);
}

Block VanillaBlocks::OXIDIZED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_BULB);
}

Block VanillaBlocks::MAGENTA_WOOL() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_WOOL);
}

Block VanillaBlocks::OXIDIZED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_BARS);
}

Block VanillaBlocks::MAGENTA_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::QUARTZ_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_DOUBLE_SLAB);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BRICK_WALL);
}

Block VanillaBlocks::MANGROVE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_SLAB);
}

Block VanillaBlocks::ORANGE_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::ORANGE_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::HARD_BROWN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_BROWN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::SMOOTH_BASALT() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_BASALT);
}

Block VanillaBlocks::WATERLILY() {
    return buildFromTypeId(BlockTypeIds::WATERLILY);
}

Block VanillaBlocks::STRIPPED_PALE_OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_PALE_OAK_WOOD);
}

Block VanillaBlocks::HARD_LIGHT_BLUE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_LIGHT_BLUE_STAINED_GLASS);
}

Block VanillaBlocks::EMERALD_BLOCK() {
    return buildFromTypeId(BlockTypeIds::EMERALD_BLOCK);
}

Block VanillaBlocks::SUSPICIOUS_SAND() {
    return buildFromTypeId(BlockTypeIds::SUSPICIOUS_SAND);
}

Block VanillaBlocks::MOSSY_COBBLESTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::MOSSY_COBBLESTONE_WALL);
}

Block VanillaBlocks::HEAVY_WEIGHTED_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::HEAVY_WEIGHTED_PRESSURE_PLATE);
}

Block VanillaBlocks::POPLAR_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::POPLAR_HANGING_SIGN);
}

Block VanillaBlocks::PURPLE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::PURPLE_STAINED_GLASS);
}

Block VanillaBlocks::LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::LIGHTNING_ROD);
}

Block VanillaBlocks::ACACIA_LEAVES() {
    return buildFromTypeId(BlockTypeIds::ACACIA_LEAVES);
}

Block VanillaBlocks::BLACK_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::BLACK_STAINED_GLASS_PANE);
}

Block VanillaBlocks::COBBLESTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::COBBLESTONE_WALL);
}

Block VanillaBlocks::UNDERWATER_TORCH() {
    return buildFromTypeId(BlockTypeIds::UNDERWATER_TORCH);
}

Block VanillaBlocks::DEEPSLATE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::SPRUCE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_DOUBLE_SLAB);
}

Block VanillaBlocks::BAMBOO_MOSAIC_SLAB() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_MOSAIC_SLAB);
}

Block VanillaBlocks::DARK_OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_LOG);
}

Block VanillaBlocks::ACACIA_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::ACACIA_HANGING_SIGN);
}

Block VanillaBlocks::OCHRE_FROGLIGHT() {
    return buildFromTypeId(BlockTypeIds::OCHRE_FROGLIGHT);
}

Block VanillaBlocks::TUFF_WALL() {
    return buildFromTypeId(BlockTypeIds::TUFF_WALL);
}

Block VanillaBlocks::OBSERVER() {
    return buildFromTypeId(BlockTypeIds::OBSERVER);
}

Block VanillaBlocks::REDSTONE_TORCH() {
    return buildFromTypeId(BlockTypeIds::REDSTONE_TORCH);
}

Block VanillaBlocks::SILVER_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::SILVER_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::GRANITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::GRANITE_STAIRS);
}

Block VanillaBlocks::PINK_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::PINK_CONCRETE);
}

Block VanillaBlocks::DARK_OAK_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_HANGING_SIGN);
}

Block VanillaBlocks::GLOWINGOBSIDIAN() {
    return buildFromTypeId(BlockTypeIds::GLOWINGOBSIDIAN);
}

Block VanillaBlocks::BROWN_MUSHROOM() {
    return buildFromTypeId(BlockTypeIds::BROWN_MUSHROOM);
}

Block VanillaBlocks::CYAN_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::CYAN_CONCRETE_POWDER);
}

Block VanillaBlocks::DEAD_FIRE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_FIRE_CORAL_WALL_FAN);
}

Block VanillaBlocks::BROWN_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BROWN_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::WAXED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::SPRUCE_SHELF() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_SHELF);
}

Block VanillaBlocks::RESIN_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RESIN_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::OXIDIZED_COPPER() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER);
}

Block VanillaBlocks::COPPER_ORE() {
    return buildFromTypeId(BlockTypeIds::COPPER_ORE);
}

Block VanillaBlocks::DARK_OAK_PLANKS() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_PLANKS);
}

Block VanillaBlocks::SULFUR_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SULFUR_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::BIRCH_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::BIRCH_PRESSURE_PLATE);
}

Block VanillaBlocks::SCAFFOLDING() {
    return buildFromTypeId(BlockTypeIds::SCAFFOLDING);
}

Block VanillaBlocks::SANDSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SANDSTONE_STAIRS);
}

Block VanillaBlocks::GREEN_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::GREEN_CANDLE_CAKE);
}

Block VanillaBlocks::STRIPPED_BAMBOO_BLOCK() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_BAMBOO_BLOCK);
}

Block VanillaBlocks::RED_MUSHROOM_BLOCK() {
    return buildFromTypeId(BlockTypeIds::RED_MUSHROOM_BLOCK);
}

Block VanillaBlocks::CRACKED_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CRACKED_STONE_BRICKS);
}

Block VanillaBlocks::SCULK_CATALYST() {
    return buildFromTypeId(BlockTypeIds::SCULK_CATALYST);
}

Block VanillaBlocks::COBBLESTONE() {
    return buildFromTypeId(BlockTypeIds::COBBLESTONE);
}

Block VanillaBlocks::WAXED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::WAXED_LIGHTNING_ROD);
}

Block VanillaBlocks::HORN_CORAL() {
    return buildFromTypeId(BlockTypeIds::HORN_CORAL);
}

Block VanillaBlocks::YELLOW_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CONCRETE);
}

Block VanillaBlocks::MANGROVE_SHELF() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_SHELF);
}

Block VanillaBlocks::CYAN_CARPET() {
    return buildFromTypeId(BlockTypeIds::CYAN_CARPET);
}

Block VanillaBlocks::WARPED_SHELF() {
    return buildFromTypeId(BlockTypeIds::WARPED_SHELF);
}

Block VanillaBlocks::OAK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::OAK_DOUBLE_SLAB);
}

Block VanillaBlocks::SMOOTH_SANDSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_SANDSTONE_STAIRS);
}

Block VanillaBlocks::JUNGLE_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_PRESSURE_PLATE);
}

Block VanillaBlocks::DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::CHALKBOARD() {
    return buildFromTypeId(BlockTypeIds::CHALKBOARD);
}

Block VanillaBlocks::BLUE_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BLUE_TERRACOTTA);
}

Block VanillaBlocks::SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::SANDSTONE);
}

Block VanillaBlocks::BROWN_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::BROWN_CANDLE_CAKE);
}

Block VanillaBlocks::ACACIA_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::ACACIA_WALL_SIGN);
}

Block VanillaBlocks::LIGHT_WEIGHTED_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_WEIGHTED_PRESSURE_PLATE);
}

Block VanillaBlocks::UNDYED_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::UNDYED_SHULKER_BOX);
}

Block VanillaBlocks::POLISHED_BLACKSTONE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE);
}

Block VanillaBlocks::MYCELIUM() {
    return buildFromTypeId(BlockTypeIds::MYCELIUM);
}

Block VanillaBlocks::EXPOSED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_LIGHTNING_ROD);
}

Block VanillaBlocks::BAMBOO() {
    return buildFromTypeId(BlockTypeIds::BAMBOO);
}

Block VanillaBlocks::QUARTZ_BLOCK() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_BLOCK);
}

Block VanillaBlocks::PALE_OAK_PLANKS() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_PLANKS);
}

Block VanillaBlocks::STONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::STONE_STAIRS);
}

Block VanillaBlocks::WAXED_WEATHERED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_CHISELED_COPPER);
}

Block VanillaBlocks::GRAY_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::GRAY_STAINED_GLASS);
}

Block VanillaBlocks::GREEN_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::GREEN_TERRACOTTA);
}

Block VanillaBlocks::DEEPSLATE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_BRICK_SLAB);
}

Block VanillaBlocks::WARPED_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WARPED_STAIRS);
}

Block VanillaBlocks::SMITHING_TABLE() {
    return buildFromTypeId(BlockTypeIds::SMITHING_TABLE);
}

Block VanillaBlocks::PLAYER_HEAD() {
    return buildFromTypeId(BlockTypeIds::PLAYER_HEAD);
}

Block VanillaBlocks::WEATHERED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_GRATE);
}

Block VanillaBlocks::POPPY() {
    return buildFromTypeId(BlockTypeIds::POPPY);
}

Block VanillaBlocks::SULFUR_SLAB() {
    return buildFromTypeId(BlockTypeIds::SULFUR_SLAB);
}

Block VanillaBlocks::TUFF_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::TUFF_BRICK_SLAB);
}

Block VanillaBlocks::COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::COPPER_CHAIN);
}

Block VanillaBlocks::COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::COPPER_CHEST);
}

Block VanillaBlocks::MOSSY_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::MOSSY_STONE_BRICKS);
}

Block VanillaBlocks::GREEN_WOOL() {
    return buildFromTypeId(BlockTypeIds::GREEN_WOOL);
}

Block VanillaBlocks::GREEN_CARPET() {
    return buildFromTypeId(BlockTypeIds::GREEN_CARPET);
}

Block VanillaBlocks::PRISMARINE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_BRICK_SLAB);
}

Block VanillaBlocks::WOODEN_DOOR() {
    return buildFromTypeId(BlockTypeIds::WOODEN_DOOR);
}

Block VanillaBlocks::PITCHER_PLANT() {
    return buildFromTypeId(BlockTypeIds::PITCHER_PLANT);
}

Block VanillaBlocks::COMPOUND_CREATOR() {
    return buildFromTypeId(BlockTypeIds::COMPOUND_CREATOR);
}

Block VanillaBlocks::SPRUCE_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_PRESSURE_PLATE);
}

Block VanillaBlocks::NETHERITE_BLOCK() {
    return buildFromTypeId(BlockTypeIds::NETHERITE_BLOCK);
}

Block VanillaBlocks::PINK_WOOL() {
    return buildFromTypeId(BlockTypeIds::PINK_WOOL);
}

Block VanillaBlocks::REDSTONE_BLOCK() {
    return buildFromTypeId(BlockTypeIds::REDSTONE_BLOCK);
}

Block VanillaBlocks::BIRCH_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::BIRCH_FENCE_GATE);
}

Block VanillaBlocks::REDSTONE_WIRE() {
    return buildFromTypeId(BlockTypeIds::REDSTONE_WIRE);
}

Block VanillaBlocks::QUARTZ_PILLAR() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_PILLAR);
}

Block VanillaBlocks::WAXED_EXPOSED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_CUT_COPPER);
}

Block VanillaBlocks::LAVA() {
    return buildFromTypeId(BlockTypeIds::LAVA);
}

Block VanillaBlocks::JUNGLE_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_HANGING_SIGN);
}

Block VanillaBlocks::BIRCH_SLAB() {
    return buildFromTypeId(BlockTypeIds::BIRCH_SLAB);
}

Block VanillaBlocks::LOOM() {
    return buildFromTypeId(BlockTypeIds::LOOM);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_LANTERN);
}

Block VanillaBlocks::DEAD_TUBE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DEAD_TUBE_CORAL_BLOCK);
}

Block VanillaBlocks::END_STONE() {
    return buildFromTypeId(BlockTypeIds::END_STONE);
}

Block VanillaBlocks::POLISHED_TUFF_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_TUFF_DOUBLE_SLAB);
}

Block VanillaBlocks::CRIMSON_DOOR() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_DOOR);
}

Block VanillaBlocks::MANGROVE_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_PRESSURE_PLATE);
}

Block VanillaBlocks::JUNGLE_SHELF() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_SHELF);
}

Block VanillaBlocks::JUNGLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::GLOWSTONE() {
    return buildFromTypeId(BlockTypeIds::GLOWSTONE);
}

Block VanillaBlocks::STONE_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::STONE_PRESSURE_PLATE);
}

Block VanillaBlocks::WAXED_EXPOSED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::HARD_WHITE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_WHITE_STAINED_GLASS);
}

Block VanillaBlocks::MUD_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::MUD_BRICK_SLAB);
}

Block VanillaBlocks::WAXED_EXPOSED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_LIGHTNING_ROD);
}

Block VanillaBlocks::EXPOSED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_LANTERN);
}

Block VanillaBlocks::FARMLAND() {
    return buildFromTypeId(BlockTypeIds::FARMLAND);
}

Block VanillaBlocks::DEAD_BRAIN_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_BRAIN_CORAL_WALL_FAN);
}

Block VanillaBlocks::CUT_RED_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::CUT_RED_SANDSTONE);
}

Block VanillaBlocks::RAIL() {
    return buildFromTypeId(BlockTypeIds::RAIL);
}

Block VanillaBlocks::BLACKSTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::BLACKSTONE_WALL);
}

Block VanillaBlocks::STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::STONE_BRICKS);
}

Block VanillaBlocks::MOSSY_COBBLESTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MOSSY_COBBLESTONE_STAIRS);
}

Block VanillaBlocks::HARD_MAGENTA_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_MAGENTA_STAINED_GLASS);
}

Block VanillaBlocks::DETECTOR_RAIL() {
    return buildFromTypeId(BlockTypeIds::DETECTOR_RAIL);
}

Block VanillaBlocks::BLUE_ORCHID() {
    return buildFromTypeId(BlockTypeIds::BLUE_ORCHID);
}

Block VanillaBlocks::GREEN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::GREEN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::POLISHED_GRANITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_GRANITE_STAIRS);
}

Block VanillaBlocks::BIRCH_LEAVES() {
    return buildFromTypeId(BlockTypeIds::BIRCH_LEAVES);
}

Block VanillaBlocks::PINK_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::PINK_TERRACOTTA);
}

Block VanillaBlocks::DARK_OAK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_DOUBLE_SLAB);
}

Block VanillaBlocks::INFESTED_COBBLESTONE() {
    return buildFromTypeId(BlockTypeIds::INFESTED_COBBLESTONE);
}

Block VanillaBlocks::PINK_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::PINK_CANDLE_CAKE);
}

Block VanillaBlocks::CRACKED_DEEPSLATE_TILES() {
    return buildFromTypeId(BlockTypeIds::CRACKED_DEEPSLATE_TILES);
}

Block VanillaBlocks::BRAIN_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::BRAIN_CORAL_WALL_FAN);
}

Block VanillaBlocks::MANGROVE_WOOD() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_WOOD);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::RED_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::RED_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_CHEST);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_CHAIN);
}

Block VanillaBlocks::DARK_OAK_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_FENCE_GATE);
}

Block VanillaBlocks::MOSSY_COBBLESTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MOSSY_COBBLESTONE_SLAB);
}

Block VanillaBlocks::BAMBOO_MOSAIC_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_MOSAIC_DOUBLE_SLAB);
}

Block VanillaBlocks::COBBLESTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::COBBLESTONE_SLAB);
}

Block VanillaBlocks::CRIMSON_NYLIUM() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_NYLIUM);
}

Block VanillaBlocks::STRUCTURE_VOID() {
    return buildFromTypeId(BlockTypeIds::STRUCTURE_VOID);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_BARS);
}

Block VanillaBlocks::PURPLE_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CONCRETE);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_BULB);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BRICK_SLAB);
}

Block VanillaBlocks::NORMAL_STONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::NORMAL_STONE_SLAB);
}

Block VanillaBlocks::SULFUR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SULFUR_STAIRS);
}

Block VanillaBlocks::HARD_YELLOW_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_YELLOW_STAINED_GLASS_PANE);
}

Block VanillaBlocks::SPRUCE_SAPLING() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_SAPLING);
}

Block VanillaBlocks::YELLOW_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::YELLOW_TERRACOTTA);
}

Block VanillaBlocks::SNOW() {
    return buildFromTypeId(BlockTypeIds::SNOW);
}

Block VanillaBlocks::SAND() {
    return buildFromTypeId(BlockTypeIds::SAND);
}

Block VanillaBlocks::DAYLIGHT_DETECTOR() {
    return buildFromTypeId(BlockTypeIds::DAYLIGHT_DETECTOR);
}

Block VanillaBlocks::MANGROVE_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_STANDING_SIGN);
}

Block VanillaBlocks::STRIPPED_MANGROVE_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_MANGROVE_WOOD);
}

Block VanillaBlocks::CONDUIT() {
    return buildFromTypeId(BlockTypeIds::CONDUIT);
}

Block VanillaBlocks::SLIME() {
    return buildFromTypeId(BlockTypeIds::SLIME);
}

Block VanillaBlocks::COPPER_TORCH() {
    return buildFromTypeId(BlockTypeIds::COPPER_TORCH);
}

Block VanillaBlocks::BONE_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BONE_BLOCK);
}

Block VanillaBlocks::FRAME() {
    return buildFromTypeId(BlockTypeIds::FRAME);
}

Block VanillaBlocks::SPRUCE_LOG() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_LOG);
}

Block VanillaBlocks::LAPIS_BLOCK() {
    return buildFromTypeId(BlockTypeIds::LAPIS_BLOCK);
}

Block VanillaBlocks::COAL_ORE() {
    return buildFromTypeId(BlockTypeIds::COAL_ORE);
}

Block VanillaBlocks::CINNABAR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_DOUBLE_SLAB);
}

Block VanillaBlocks::MOSSY_STONE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MOSSY_STONE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::CUT_RED_SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CUT_RED_SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::CLIENT_REQUEST_PLACEHOLDER_BLOCK() {
    return buildFromTypeId(BlockTypeIds::CLIENT_REQUEST_PLACEHOLDER_BLOCK);
}

Block VanillaBlocks::BAMBOO_SHELF() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_SHELF);
}

Block VanillaBlocks::REDSTONE_ORE() {
    return buildFromTypeId(BlockTypeIds::REDSTONE_ORE);
}

Block VanillaBlocks::BAMBOO_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_DOUBLE_SLAB);
}

Block VanillaBlocks::WAXED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_CHEST);
}

Block VanillaBlocks::GREEN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::GREEN_STAINED_GLASS);
}

Block VanillaBlocks::WAXED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_CHAIN);
}

Block VanillaBlocks::BUBBLE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BUBBLE_CORAL_BLOCK);
}

Block VanillaBlocks::INFESTED_CHISELED_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::INFESTED_CHISELED_STONE_BRICKS);
}

Block VanillaBlocks::POLISHED_CINNABAR_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_CINNABAR_WALL);
}

Block VanillaBlocks::NETHER_BRICK_FENCE() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK_FENCE);
}

Block VanillaBlocks::PINK_TULIP() {
    return buildFromTypeId(BlockTypeIds::PINK_TULIP);
}

Block VanillaBlocks::OAK_SLAB() {
    return buildFromTypeId(BlockTypeIds::OAK_SLAB);
}

Block VanillaBlocks::STRIPPED_PALE_OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_PALE_OAK_LOG);
}

Block VanillaBlocks::DEEPSLATE_TILE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_TILE_SLAB);
}

Block VanillaBlocks::PINK_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::PINK_CONCRETE_POWDER);
}

Block VanillaBlocks::PALE_OAK_SLAB() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_SLAB);
}

Block VanillaBlocks::DEAD_TUBE_CORAL() {
    return buildFromTypeId(BlockTypeIds::DEAD_TUBE_CORAL);
}

Block VanillaBlocks::NETHER_WART_BLOCK() {
    return buildFromTypeId(BlockTypeIds::NETHER_WART_BLOCK);
}

Block VanillaBlocks::PRISMARINE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_SLAB);
}

Block VanillaBlocks::PRISMARINE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_DOUBLE_SLAB);
}

Block VanillaBlocks::CHERRY_DOOR() {
    return buildFromTypeId(BlockTypeIds::CHERRY_DOOR);
}

Block VanillaBlocks::COLORED_TORCH_BLUE() {
    return buildFromTypeId(BlockTypeIds::COLORED_TORCH_BLUE);
}

Block VanillaBlocks::CRIMSON_HYPHAE() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_HYPHAE);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_STAIRS);
}

Block VanillaBlocks::WEATHERED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::SMALL_DRIPLEAF_BLOCK() {
    return buildFromTypeId(BlockTypeIds::SMALL_DRIPLEAF_BLOCK);
}

Block VanillaBlocks::POLISHED_SULFUR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_SULFUR_STAIRS);
}

Block VanillaBlocks::PINK_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::PINK_STAINED_GLASS);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_GRATE);
}

Block VanillaBlocks::SPRUCE_BUTTON() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_BUTTON);
}

Block VanillaBlocks::ACACIA_LOG() {
    return buildFromTypeId(BlockTypeIds::ACACIA_LOG);
}

Block VanillaBlocks::CRIMSON_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_TRAPDOOR);
}

Block VanillaBlocks::BASALT() {
    return buildFromTypeId(BlockTypeIds::BASALT);
}

Block VanillaBlocks::HARD_CYAN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_CYAN_STAINED_GLASS);
}

Block VanillaBlocks::NORMAL_STONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::NORMAL_STONE_DOUBLE_SLAB);
}

Block VanillaBlocks::STONE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::STONE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_TERRACOTTA);
}

Block VanillaBlocks::LIT_REDSTONE_LAMP() {
    return buildFromTypeId(BlockTypeIds::LIT_REDSTONE_LAMP);
}

Block VanillaBlocks::COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::HARD_BLUE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_BLUE_STAINED_GLASS);
}

Block VanillaBlocks::HARD_PURPLE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_PURPLE_STAINED_GLASS);
}

Block VanillaBlocks::DIAMOND_ORE() {
    return buildFromTypeId(BlockTypeIds::DIAMOND_ORE);
}

Block VanillaBlocks::WARPED_ROOTS() {
    return buildFromTypeId(BlockTypeIds::WARPED_ROOTS);
}

Block VanillaBlocks::MAGENTA_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CONCRETE);
}

Block VanillaBlocks::DARK_PRISMARINE() {
    return buildFromTypeId(BlockTypeIds::DARK_PRISMARINE);
}

Block VanillaBlocks::STICKY_PISTON() {
    return buildFromTypeId(BlockTypeIds::STICKY_PISTON);
}

Block VanillaBlocks::ENDER_CHEST() {
    return buildFromTypeId(BlockTypeIds::ENDER_CHEST);
}

Block VanillaBlocks::MEDIUM_AMETHYST_BUD() {
    return buildFromTypeId(BlockTypeIds::MEDIUM_AMETHYST_BUD);
}

Block VanillaBlocks::PINK_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::PINK_SHULKER_BOX);
}

Block VanillaBlocks::WARPED_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::WARPED_DOUBLE_SLAB);
}

Block VanillaBlocks::JUNGLE_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_WALL_SIGN);
}

Block VanillaBlocks::SCULK_SENSOR() {
    return buildFromTypeId(BlockTypeIds::SCULK_SENSOR);
}

Block VanillaBlocks::COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::COPPER_BULB);
}

Block VanillaBlocks::COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::COPPER_BARS);
}

Block VanillaBlocks::OAK_SHELF() {
    return buildFromTypeId(BlockTypeIds::OAK_SHELF);
}

Block VanillaBlocks::DIORITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::DIORITE_STAIRS);
}

Block VanillaBlocks::SPRUCE_LEAVES() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_LEAVES);
}

Block VanillaBlocks::FROG_SPAWN() {
    return buildFromTypeId(BlockTypeIds::FROG_SPAWN);
}

Block VanillaBlocks::ACACIA_DOOR() {
    return buildFromTypeId(BlockTypeIds::ACACIA_DOOR);
}

Block VanillaBlocks::SMOOTH_SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::RED_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::RED_SHULKER_BOX);
}

Block VanillaBlocks::STRIPPED_CHERRY_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_CHERRY_LOG);
}

Block VanillaBlocks::CRIMSON_BUTTON() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_BUTTON);
}

Block VanillaBlocks::ACACIA_PLANKS() {
    return buildFromTypeId(BlockTypeIds::ACACIA_PLANKS);
}

Block VanillaBlocks::FIRE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::FIRE_CORAL_BLOCK);
}

Block VanillaBlocks::MAGENTA_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CONCRETE_POWDER);
}

Block VanillaBlocks::IRON_DOOR() {
    return buildFromTypeId(BlockTypeIds::IRON_DOOR);
}

Block VanillaBlocks::HONEYCOMB_BLOCK() {
    return buildFromTypeId(BlockTypeIds::HONEYCOMB_BLOCK);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BRICK_STAIRS);
}

Block VanillaBlocks::MANGROVE_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_TRAPDOOR);
}

Block VanillaBlocks::QUARTZ_ORE() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_ORE);
}

Block VanillaBlocks::DAYLIGHT_DETECTOR_INVERTED() {
    return buildFromTypeId(BlockTypeIds::DAYLIGHT_DETECTOR_INVERTED);
}

Block VanillaBlocks::BARREL() {
    return buildFromTypeId(BlockTypeIds::BARREL);
}

Block VanillaBlocks::SMOOTH_QUARTZ() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_QUARTZ);
}

Block VanillaBlocks::COARSE_DIRT() {
    return buildFromTypeId(BlockTypeIds::COARSE_DIRT);
}

Block VanillaBlocks::CHORUS_FLOWER() {
    return buildFromTypeId(BlockTypeIds::CHORUS_FLOWER);
}

Block VanillaBlocks::ORANGE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::ORANGE_STAINED_GLASS);
}

Block VanillaBlocks::WHITE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::WHITE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::CINNABAR_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::POLISHED_CINNABAR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_CINNABAR_DOUBLE_SLAB);
}

Block VanillaBlocks::SULFUR_WALL() {
    return buildFromTypeId(BlockTypeIds::SULFUR_WALL);
}

Block VanillaBlocks::SULFUR_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SULFUR_BRICK_STAIRS);
}

Block VanillaBlocks::STRIPPED_BIRCH_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_BIRCH_WOOD);
}

Block VanillaBlocks::CRACKED_NETHER_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CRACKED_NETHER_BRICKS);
}

Block VanillaBlocks::POWERED_REPEATER() {
    return buildFromTypeId(BlockTypeIds::POWERED_REPEATER);
}

Block VanillaBlocks::LIGHT_BLUE_CANDLE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CANDLE);
}

Block VanillaBlocks::HARD_LIME_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_LIME_STAINED_GLASS_PANE);
}

Block VanillaBlocks::PUMPKIN() {
    return buildFromTypeId(BlockTypeIds::PUMPKIN);
}

Block VanillaBlocks::ELEMENT_CONSTRUCTOR() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_CONSTRUCTOR);
}

Block VanillaBlocks::DEEPSLATE_TILES() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_TILES);
}

Block VanillaBlocks::SMOOTH_STONE() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_STONE);
}

Block VanillaBlocks::HARD_LIGHT_GRAY_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_LIGHT_GRAY_STAINED_GLASS_PANE);
}

Block VanillaBlocks::GRAY_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::GRAY_TERRACOTTA);
}

Block VanillaBlocks::OXIDIZED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::GRANITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRANITE_SLAB);
}

Block VanillaBlocks::WHITE_TULIP() {
    return buildFromTypeId(BlockTypeIds::WHITE_TULIP);
}

Block VanillaBlocks::LIME_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::LIME_CONCRETE);
}

Block VanillaBlocks::BLACK_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::BLACK_CANDLE_CAKE);
}

Block VanillaBlocks::RED_MUSHROOM() {
    return buildFromTypeId(BlockTypeIds::RED_MUSHROOM);
}

Block VanillaBlocks::GILDED_BLACKSTONE() {
    return buildFromTypeId(BlockTypeIds::GILDED_BLACKSTONE);
}

Block VanillaBlocks::HARD_YELLOW_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_YELLOW_STAINED_GLASS);
}

Block VanillaBlocks::MAGENTA_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_TERRACOTTA);
}

Block VanillaBlocks::EXPOSED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::MANGROVE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_STAIRS);
}

Block VanillaBlocks::POLISHED_DIORITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DIORITE_SLAB);
}

Block VanillaBlocks::RESERVED6() {
    return buildFromTypeId(BlockTypeIds::RESERVED6);
}

Block VanillaBlocks::CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CUT_COPPER_STAIRS);
}

Block VanillaBlocks::LAB_TABLE() {
    return buildFromTypeId(BlockTypeIds::LAB_TABLE);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_LANTERN);
}

Block VanillaBlocks::CHERRY_BUTTON() {
    return buildFromTypeId(BlockTypeIds::CHERRY_BUTTON);
}

Block VanillaBlocks::YELLOW_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CANDLE_CAKE);
}

Block VanillaBlocks::MANGROVE_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_FENCE_GATE);
}

Block VanillaBlocks::SUNFLOWER() {
    return buildFromTypeId(BlockTypeIds::SUNFLOWER);
}

Block VanillaBlocks::PINK_PETALS() {
    return buildFromTypeId(BlockTypeIds::PINK_PETALS);
}

Block VanillaBlocks::BAMBOO_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_HANGING_SIGN);
}

Block VanillaBlocks::INFESTED_DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::INFESTED_DEEPSLATE);
}

Block VanillaBlocks::SOUL_TORCH() {
    return buildFromTypeId(BlockTypeIds::SOUL_TORCH);
}

Block VanillaBlocks::PODZOL() {
    return buildFromTypeId(BlockTypeIds::PODZOL);
}

Block VanillaBlocks::COPPER_BLOCK() {
    return buildFromTypeId(BlockTypeIds::COPPER_BLOCK);
}

Block VanillaBlocks::LIT_REDSTONE_ORE() {
    return buildFromTypeId(BlockTypeIds::LIT_REDSTONE_ORE);
}

Block VanillaBlocks::DEEPSLATE_TILE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_TILE_STAIRS);
}

Block VanillaBlocks::CRIMSON_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_FENCE_GATE);
}

Block VanillaBlocks::DEADBUSH() {
    return buildFromTypeId(BlockTypeIds::DEADBUSH);
}

Block VanillaBlocks::WAXED_WEATHERED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BRICKS);
}

Block VanillaBlocks::RED_CANDLE() {
    return buildFromTypeId(BlockTypeIds::RED_CANDLE);
}

Block VanillaBlocks::CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::CUT_COPPER);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::IRON_ORE() {
    return buildFromTypeId(BlockTypeIds::IRON_ORE);
}

Block VanillaBlocks::SPRUCE_DOOR() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_DOOR);
}

Block VanillaBlocks::FROSTED_ICE() {
    return buildFromTypeId(BlockTypeIds::FROSTED_ICE);
}

Block VanillaBlocks::CHIPPED_ANVIL() {
    return buildFromTypeId(BlockTypeIds::CHIPPED_ANVIL);
}

Block VanillaBlocks::LARGE_AMETHYST_BUD() {
    return buildFromTypeId(BlockTypeIds::LARGE_AMETHYST_BUD);
}

Block VanillaBlocks::EXPOSED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_DOOR);
}

Block VanillaBlocks::SUSPICIOUS_GRAVEL() {
    return buildFromTypeId(BlockTypeIds::SUSPICIOUS_GRAVEL);
}

Block VanillaBlocks::WARPED_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WARPED_TRAPDOOR);
}

Block VanillaBlocks::FLOWING_WATER() {
    return buildFromTypeId(BlockTypeIds::FLOWING_WATER);
}

Block VanillaBlocks::BRICK_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BRICK_BLOCK);
}

Block VanillaBlocks::HARD_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_GLASS);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::QUARTZ_STAIRS() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_STAIRS);
}

Block VanillaBlocks::CAVE_VINES() {
    return buildFromTypeId(BlockTypeIds::CAVE_VINES);
}

Block VanillaBlocks::MAGENTA_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_STAINED_GLASS_PANE);
}

Block VanillaBlocks::IRON_BARS() {
    return buildFromTypeId(BlockTypeIds::IRON_BARS);
}

Block VanillaBlocks::WHITE_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::WHITE_TERRACOTTA);
}

Block VanillaBlocks::STRIPPED_OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_OAK_WOOD);
}

Block VanillaBlocks::LIGHT_BLUE_CARPET() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CARPET);
}

Block VanillaBlocks::OAK_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::OAK_HANGING_SIGN);
}

Block VanillaBlocks::WHITE_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::WHITE_CONCRETE_POWDER);
}

Block VanillaBlocks::MELON_STEM() {
    return buildFromTypeId(BlockTypeIds::MELON_STEM);
}

Block VanillaBlocks::CRIMSON_PLANKS() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_PLANKS);
}

Block VanillaBlocks::STRIPPED_DARK_OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_DARK_OAK_WOOD);
}

Block VanillaBlocks::WAXED_WEATHERED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_CUT_COPPER);
}

Block VanillaBlocks::WHITE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::WHITE_STAINED_GLASS);
}

Block VanillaBlocks::HORN_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::HORN_CORAL_WALL_FAN);
}

Block VanillaBlocks::OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::OAK_WOOD);
}

Block VanillaBlocks::PURPLE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::PURPLE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::WALL_SIGN);
}

Block VanillaBlocks::JUKEBOX() {
    return buildFromTypeId(BlockTypeIds::JUKEBOX);
}

Block VanillaBlocks::STRIPPED_CHERRY_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_CHERRY_WOOD);
}

Block VanillaBlocks::JIGSAW() {
    return buildFromTypeId(BlockTypeIds::JIGSAW);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::CHISELED_SULFUR() {
    return buildFromTypeId(BlockTypeIds::CHISELED_SULFUR);
}

Block VanillaBlocks::POLISHED_CINNABAR_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_CINNABAR_SLAB);
}

Block VanillaBlocks::PRISMARINE_WALL() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_WALL);
}

Block VanillaBlocks::BORDER_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BORDER_BLOCK);
}

Block VanillaBlocks::SHROOMLIGHT() {
    return buildFromTypeId(BlockTypeIds::SHROOMLIGHT);
}

Block VanillaBlocks::BAMBOO_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_FENCE_GATE);
}

Block VanillaBlocks::CORNFLOWER() {
    return buildFromTypeId(BlockTypeIds::CORNFLOWER);
}

Block VanillaBlocks::CHISELED_POLISHED_BLACKSTONE() {
    return buildFromTypeId(BlockTypeIds::CHISELED_POLISHED_BLACKSTONE);
}

Block VanillaBlocks::DARK_OAK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_STAIRS);
}

Block VanillaBlocks::DEEPSLATE_TILE_WALL() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_TILE_WALL);
}

Block VanillaBlocks::GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::GLASS_PANE);
}

Block VanillaBlocks::CHISELED_DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::CHISELED_DEEPSLATE);
}

Block VanillaBlocks::CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::CUT_COPPER_SLAB);
}

Block VanillaBlocks::POLISHED_SULFUR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_SULFUR_DOUBLE_SLAB);
}

Block VanillaBlocks::RED_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::RED_STAINED_GLASS);
}

Block VanillaBlocks::PALE_OAK_WOOD() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_WOOD);
}

Block VanillaBlocks::INFESTED_STONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::INFESTED_STONE_BRICKS);
}

Block VanillaBlocks::ACACIA_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::ACACIA_PRESSURE_PLATE);
}

Block VanillaBlocks::WEATHERED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_LIGHTNING_ROD);
}

Block VanillaBlocks::BAMBOO_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_TRAPDOOR);
}

Block VanillaBlocks::OXIDIZED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_CHISELED_COPPER);
}

Block VanillaBlocks::MANGROVE_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_WALL_SIGN);
}

Block VanillaBlocks::RAW_COPPER_BLOCK() {
    return buildFromTypeId(BlockTypeIds::RAW_COPPER_BLOCK);
}

Block VanillaBlocks::TALL_DRY_GRASS() {
    return buildFromTypeId(BlockTypeIds::TALL_DRY_GRASS);
}

Block VanillaBlocks::OXIDIZED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::HORN_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::HORN_CORAL_BLOCK);
}

Block VanillaBlocks::DARK_OAK_SHELF() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_SHELF);
}

Block VanillaBlocks::BEETROOT() {
    return buildFromTypeId(BlockTypeIds::BEETROOT);
}

Block VanillaBlocks::LIGHT_GRAY_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CANDLE_CAKE);
}

Block VanillaBlocks::WHITE_CANDLE() {
    return buildFromTypeId(BlockTypeIds::WHITE_CANDLE);
}

Block VanillaBlocks::ANDESITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::ANDESITE_STAIRS);
}

Block VanillaBlocks::BIRCH_PLANKS() {
    return buildFromTypeId(BlockTypeIds::BIRCH_PLANKS);
}

Block VanillaBlocks::GOLDEN_RAIL() {
    return buildFromTypeId(BlockTypeIds::GOLDEN_RAIL);
}

Block VanillaBlocks::CYAN_WOOL() {
    return buildFromTypeId(BlockTypeIds::CYAN_WOOL);
}

Block VanillaBlocks::PETRIFIED_OAK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PETRIFIED_OAK_DOUBLE_SLAB);
}

Block VanillaBlocks::DEPRECATED_ANVIL() {
    return buildFromTypeId(BlockTypeIds::DEPRECATED_ANVIL);
}

Block VanillaBlocks::DARKOAK_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::DARKOAK_WALL_SIGN);
}

Block VanillaBlocks::JUNGLE_LEAVES() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_LEAVES);
}

Block VanillaBlocks::GRAY_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::GRAY_SHULKER_BOX);
}

Block VanillaBlocks::RED_SANDSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::RED_SANDSTONE_STAIRS);
}

Block VanillaBlocks::CYAN_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::CYAN_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::CRACKED_DEEPSLATE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CRACKED_DEEPSLATE_BRICKS);
}

Block VanillaBlocks::FIRE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::FIRE_CORAL_WALL_FAN);
}

Block VanillaBlocks::JUNGLE_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_FENCE_GATE);
}

Block VanillaBlocks::EXPOSED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_GRATE);
}

Block VanillaBlocks::WAXED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_GRATE);
}

Block VanillaBlocks::HARD_LIGHT_BLUE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_LIGHT_BLUE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::JUNGLE_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_TRAPDOOR);
}

Block VanillaBlocks::DIRT_WITH_ROOTS() {
    return buildFromTypeId(BlockTypeIds::DIRT_WITH_ROOTS);
}

Block VanillaBlocks::COAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::COAL_BLOCK);
}

Block VanillaBlocks::WHITE_WOOL() {
    return buildFromTypeId(BlockTypeIds::WHITE_WOOL);
}

Block VanillaBlocks::WARPED_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::WARPED_FENCE_GATE);
}

Block VanillaBlocks::CUT_SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CUT_SANDSTONE_SLAB);
}

Block VanillaBlocks::SKELETON_SKULL() {
    return buildFromTypeId(BlockTypeIds::SKELETON_SKULL);
}

Block VanillaBlocks::EXPOSED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_CHEST);
}

Block VanillaBlocks::EXPOSED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_CHAIN);
}

Block VanillaBlocks::COMPOSTER() {
    return buildFromTypeId(BlockTypeIds::COMPOSTER);
}

Block VanillaBlocks::WAXED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::KELP() {
    return buildFromTypeId(BlockTypeIds::KELP);
}

Block VanillaBlocks::SULFUR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SULFUR_DOUBLE_SLAB);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_DOOR);
}

Block VanillaBlocks::DEEPSLATE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_BRICKS);
}

Block VanillaBlocks::BLUE_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BLUE_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::LIGHT_BLUE_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::ROSE_BUSH() {
    return buildFromTypeId(BlockTypeIds::ROSE_BUSH);
}

Block VanillaBlocks::FLOWERING_AZALEA() {
    return buildFromTypeId(BlockTypeIds::FLOWERING_AZALEA);
}

Block VanillaBlocks::OXIDIZED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_CUT_COPPER);
}

Block VanillaBlocks::POPLAR_SHELF() {
    return buildFromTypeId(BlockTypeIds::POPLAR_SHELF);
}

Block VanillaBlocks::BLUE_WOOL() {
    return buildFromTypeId(BlockTypeIds::BLUE_WOOL);
}

Block VanillaBlocks::PALE_OAK_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_HANGING_SIGN);
}

Block VanillaBlocks::WEEPING_VINES() {
    return buildFromTypeId(BlockTypeIds::WEEPING_VINES);
}

Block VanillaBlocks::CHORUS_PLANT() {
    return buildFromTypeId(BlockTypeIds::CHORUS_PLANT);
}

Block VanillaBlocks::WATER() {
    return buildFromTypeId(BlockTypeIds::WATER);
}

Block VanillaBlocks::MUD_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MUD_BRICK_STAIRS);
}

Block VanillaBlocks::UNPOWERED_REPEATER() {
    return buildFromTypeId(BlockTypeIds::UNPOWERED_REPEATER);
}

Block VanillaBlocks::STONE_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::STONE_BRICK_WALL);
}

Block VanillaBlocks::SMOOTH_RED_SANDSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_RED_SANDSTONE_STAIRS);
}

Block VanillaBlocks::ELEMENT_100() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_100);
}

Block VanillaBlocks::ELEMENT_101() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_101);
}

Block VanillaBlocks::ELEMENT_102() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_102);
}

Block VanillaBlocks::ELEMENT_103() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_103);
}

Block VanillaBlocks::ELEMENT_104() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_104);
}

Block VanillaBlocks::ELEMENT_105() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_105);
}

Block VanillaBlocks::ELEMENT_106() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_106);
}

Block VanillaBlocks::ELEMENT_107() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_107);
}

Block VanillaBlocks::ELEMENT_108() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_108);
}

Block VanillaBlocks::ELEMENT_109() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_109);
}

Block VanillaBlocks::ELEMENT_113() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_113);
}

Block VanillaBlocks::ELEMENT_112() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_112);
}

Block VanillaBlocks::ELEMENT_111() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_111);
}

Block VanillaBlocks::ELEMENT_110() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_110);
}

Block VanillaBlocks::ELEMENT_117() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_117);
}

Block VanillaBlocks::ELEMENT_116() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_116);
}

Block VanillaBlocks::ELEMENT_115() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_115);
}

Block VanillaBlocks::ELEMENT_114() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_114);
}

Block VanillaBlocks::ELEMENT_118() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_118);
}

Block VanillaBlocks::ANDESITE_WALL() {
    return buildFromTypeId(BlockTypeIds::ANDESITE_WALL);
}

Block VanillaBlocks::WHITE_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::WHITE_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::STRIPPED_WARPED_HYPHAE() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_WARPED_HYPHAE);
}

Block VanillaBlocks::MOVING_BLOCK() {
    return buildFromTypeId(BlockTypeIds::MOVING_BLOCK);
}

Block VanillaBlocks::TRAPPED_CHEST() {
    return buildFromTypeId(BlockTypeIds::TRAPPED_CHEST);
}

Block VanillaBlocks::ACACIA_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::ACACIA_TRAPDOOR);
}

Block VanillaBlocks::WEATHERED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_CHEST);
}

Block VanillaBlocks::BRAIN_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BRAIN_CORAL_BLOCK);
}

Block VanillaBlocks::WEATHERED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_CHAIN);
}

Block VanillaBlocks::STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::STANDING_SIGN);
}

Block VanillaBlocks::BAMBOO_PLANKS() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_PLANKS);
}

Block VanillaBlocks::GLOW_LICHEN() {
    return buildFromTypeId(BlockTypeIds::GLOW_LICHEN);
}

Block VanillaBlocks::PURPUR_PILLAR() {
    return buildFromTypeId(BlockTypeIds::PURPUR_PILLAR);
}

Block VanillaBlocks::WALL_BANNER() {
    return buildFromTypeId(BlockTypeIds::WALL_BANNER);
}

Block VanillaBlocks::TWISTING_VINES() {
    return buildFromTypeId(BlockTypeIds::TWISTING_VINES);
}

Block VanillaBlocks::CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::CHISELED_COPPER);
}

Block VanillaBlocks::ACACIA_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ACACIA_DOUBLE_SLAB);
}

Block VanillaBlocks::DARK_OAK_DOOR() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_DOOR);
}

Block VanillaBlocks::OAK_FENCE() {
    return buildFromTypeId(BlockTypeIds::OAK_FENCE);
}

Block VanillaBlocks::PALE_MOSS_BLOCK() {
    return buildFromTypeId(BlockTypeIds::PALE_MOSS_BLOCK);
}

Block VanillaBlocks::SOUL_LANTERN() {
    return buildFromTypeId(BlockTypeIds::SOUL_LANTERN);
}

Block VanillaBlocks::DIRT() {
    return buildFromTypeId(BlockTypeIds::DIRT);
}

Block VanillaBlocks::BLUE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::BLUE_STAINED_GLASS);
}

Block VanillaBlocks::DENY() {
    return buildFromTypeId(BlockTypeIds::DENY);
}

Block VanillaBlocks::BEE_NEST() {
    return buildFromTypeId(BlockTypeIds::BEE_NEST);
}

Block VanillaBlocks::BUBBLE_COLUMN() {
    return buildFromTypeId(BlockTypeIds::BUBBLE_COLUMN);
}

Block VanillaBlocks::CAMPFIRE() {
    return buildFromTypeId(BlockTypeIds::CAMPFIRE);
}

Block VanillaBlocks::SMOOTH_STONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_STONE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_STAINED_GLASS);
}

Block VanillaBlocks::SOUL_SOIL() {
    return buildFromTypeId(BlockTypeIds::SOUL_SOIL);
}

Block VanillaBlocks::SOUL_SAND() {
    return buildFromTypeId(BlockTypeIds::SOUL_SAND);
}

Block VanillaBlocks::GRANITE_WALL() {
    return buildFromTypeId(BlockTypeIds::GRANITE_WALL);
}

Block VanillaBlocks::SPRUCE_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_HANGING_SIGN);
}

Block VanillaBlocks::POLISHED_DIORITE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DIORITE);
}

Block VanillaBlocks::REINFORCED_DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::REINFORCED_DEEPSLATE);
}

Block VanillaBlocks::FLETCHING_TABLE() {
    return buildFromTypeId(BlockTypeIds::FLETCHING_TABLE);
}

Block VanillaBlocks::CHERRY_LEAVES() {
    return buildFromTypeId(BlockTypeIds::CHERRY_LEAVES);
}

Block VanillaBlocks::CREEPER_HEAD() {
    return buildFromTypeId(BlockTypeIds::CREEPER_HEAD);
}

Block VanillaBlocks::BLACK_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BLACK_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::WAXED_OXIDIZED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_BULB);
}

Block VanillaBlocks::DRAGON_HEAD() {
    return buildFromTypeId(BlockTypeIds::DRAGON_HEAD);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_BARS);
}

Block VanillaBlocks::CALIBRATED_SCULK_SENSOR() {
    return buildFromTypeId(BlockTypeIds::CALIBRATED_SCULK_SENSOR);
}

Block VanillaBlocks::DARK_PRISMARINE_SLAB() {
    return buildFromTypeId(BlockTypeIds::DARK_PRISMARINE_SLAB);
}

Block VanillaBlocks::COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::COPPER_TRAPDOOR);
}

Block VanillaBlocks::STRIPPED_ACACIA_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_ACACIA_LOG);
}

Block VanillaBlocks::CINNABAR_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_BRICKS);
}

Block VanillaBlocks::COBBLED_DEEPSLATE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::COBBLED_DEEPSLATE_DOUBLE_SLAB);
}

Block VanillaBlocks::WARPED_FENCE() {
    return buildFromTypeId(BlockTypeIds::WARPED_FENCE);
}

Block VanillaBlocks::CRAFTING_TABLE() {
    return buildFromTypeId(BlockTypeIds::CRAFTING_TABLE);
}

Block VanillaBlocks::SEA_PICKLE() {
    return buildFromTypeId(BlockTypeIds::SEA_PICKLE);
}

Block VanillaBlocks::CHERRY_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::CHERRY_STANDING_SIGN);
}

Block VanillaBlocks::PALE_OAK_SHELF() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_SHELF);
}

Block VanillaBlocks::POTENT_SULFUR() {
    return buildFromTypeId(BlockTypeIds::POTENT_SULFUR);
}

Block VanillaBlocks::BROWN_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::BROWN_CONCRETE_POWDER);
}

Block VanillaBlocks::MANGROVE_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_HANGING_SIGN);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::BROWN_CANDLE() {
    return buildFromTypeId(BlockTypeIds::BROWN_CANDLE);
}

Block VanillaBlocks::MOSSY_STONE_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MOSSY_STONE_BRICK_STAIRS);
}

Block VanillaBlocks::END_ROD() {
    return buildFromTypeId(BlockTypeIds::END_ROD);
}

Block VanillaBlocks::CRIMSON_STEM() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_STEM);
}

Block VanillaBlocks::GREEN_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::GREEN_CONCRETE);
}

Block VanillaBlocks::TUFF_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::TUFF_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::CRIMSON_SLAB() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_SLAB);
}

Block VanillaBlocks::WARPED_HYPHAE() {
    return buildFromTypeId(BlockTypeIds::WARPED_HYPHAE);
}

Block VanillaBlocks::WARPED_WART_BLOCK() {
    return buildFromTypeId(BlockTypeIds::WARPED_WART_BLOCK);
}

Block VanillaBlocks::POPLAR_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::POPLAR_PRESSURE_PLATE);
}

Block VanillaBlocks::LIGHT_GRAY_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_SHULKER_BOX);
}

Block VanillaBlocks::RESIN_BRICKS() {
    return buildFromTypeId(BlockTypeIds::RESIN_BRICKS);
}

Block VanillaBlocks::CARROTS() {
    return buildFromTypeId(BlockTypeIds::CARROTS);
}

Block VanillaBlocks::TUFF_STAIRS() {
    return buildFromTypeId(BlockTypeIds::TUFF_STAIRS);
}

Block VanillaBlocks::YELLOW_CARPET() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CARPET);
}

Block VanillaBlocks::CYAN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::CYAN_STAINED_GLASS);
}

Block VanillaBlocks::BLACK_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::BLACK_STAINED_GLASS);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_DOOR);
}

Block VanillaBlocks::DEAD_HORN_CORAL() {
    return buildFromTypeId(BlockTypeIds::DEAD_HORN_CORAL);
}

Block VanillaBlocks::ANDESITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ANDESITE_DOUBLE_SLAB);
}

Block VanillaBlocks::GRASS_BLOCK() {
    return buildFromTypeId(BlockTypeIds::GRASS_BLOCK);
}

Block VanillaBlocks::TRIPWIRE_HOOK() {
    return buildFromTypeId(BlockTypeIds::TRIPWIRE_HOOK);
}

Block VanillaBlocks::CAVE_VINES_BODY_WITH_BERRIES() {
    return buildFromTypeId(BlockTypeIds::CAVE_VINES_BODY_WITH_BERRIES);
}

Block VanillaBlocks::DARK_OAK_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_PRESSURE_PLATE);
}

Block VanillaBlocks::COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::COPPER_DOOR);
}

Block VanillaBlocks::HARD_BLACK_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_BLACK_STAINED_GLASS);
}

Block VanillaBlocks::STRIPPED_BIRCH_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_BIRCH_LOG);
}

Block VanillaBlocks::TINTED_GLASS() {
    return buildFromTypeId(BlockTypeIds::TINTED_GLASS);
}

Block VanillaBlocks::BIG_DRIPLEAF() {
    return buildFromTypeId(BlockTypeIds::BIG_DRIPLEAF);
}

Block VanillaBlocks::CUT_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::CUT_SANDSTONE);
}

Block VanillaBlocks::WARPED_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::WARPED_HANGING_SIGN);
}

Block VanillaBlocks::LIME_WOOL() {
    return buildFromTypeId(BlockTypeIds::LIME_WOOL);
}

Block VanillaBlocks::BLUE_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::BLUE_CANDLE_CAKE);
}

Block VanillaBlocks::SWEET_BERRY_BUSH() {
    return buildFromTypeId(BlockTypeIds::SWEET_BERRY_BUSH);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_SLAB);
}

Block VanillaBlocks::REEDS() {
    return buildFromTypeId(BlockTypeIds::REEDS);
}

Block VanillaBlocks::BLACK_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::BLACK_SHULKER_BOX);
}

Block VanillaBlocks::WEATHERED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::JUNGLE_SAPLING() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_SAPLING);
}

Block VanillaBlocks::CHISELED_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::CHISELED_SANDSTONE);
}

Block VanillaBlocks::BARRIER() {
    return buildFromTypeId(BlockTypeIds::BARRIER);
}

Block VanillaBlocks::TORCHFLOWER_CROP() {
    return buildFromTypeId(BlockTypeIds::TORCHFLOWER_CROP);
}

Block VanillaBlocks::BLACK_CARPET() {
    return buildFromTypeId(BlockTypeIds::BLACK_CARPET);
}

Block VanillaBlocks::PALE_OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_LOG);
}

Block VanillaBlocks::JUNGLE_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_STANDING_SIGN);
}

Block VanillaBlocks::CHERRY_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CHERRY_DOUBLE_SLAB);
}

Block VanillaBlocks::WEATHERED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::POPLAR_DOOR() {
    return buildFromTypeId(BlockTypeIds::POPLAR_DOOR);
}

Block VanillaBlocks::OXIDIZED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_LANTERN);
}

Block VanillaBlocks::DARK_OAK_LEAVES() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_LEAVES);
}

Block VanillaBlocks::NETHER_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK_SLAB);
}

Block VanillaBlocks::FIRE() {
    return buildFromTypeId(BlockTypeIds::FIRE);
}

Block VanillaBlocks::FERN() {
    return buildFromTypeId(BlockTypeIds::FERN);
}

Block VanillaBlocks::PURPUR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPUR_DOUBLE_SLAB);
}

Block VanillaBlocks::TORCHFLOWER() {
    return buildFromTypeId(BlockTypeIds::TORCHFLOWER);
}

Block VanillaBlocks::SHORT_DRY_GRASS() {
    return buildFromTypeId(BlockTypeIds::SHORT_DRY_GRASS);
}

Block VanillaBlocks::INFESTED_STONE() {
    return buildFromTypeId(BlockTypeIds::INFESTED_STONE);
}

Block VanillaBlocks::PALE_HANGING_MOSS() {
    return buildFromTypeId(BlockTypeIds::PALE_HANGING_MOSS);
}

Block VanillaBlocks::PALE_MOSS_CARPET() {
    return buildFromTypeId(BlockTypeIds::PALE_MOSS_CARPET);
}

Block VanillaBlocks::END_PORTAL_FRAME() {
    return buildFromTypeId(BlockTypeIds::END_PORTAL_FRAME);
}

Block VanillaBlocks::BAMBOO_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_PRESSURE_PLATE);
}

Block VanillaBlocks::PRISMARINE() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE);
}

Block VanillaBlocks::MAGENTA_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CANDLE_CAKE);
}

Block VanillaBlocks::EXPOSED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::MUSHROOM_STEM() {
    return buildFromTypeId(BlockTypeIds::MUSHROOM_STEM);
}

Block VanillaBlocks::BLACK_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BLACK_TERRACOTTA);
}

Block VanillaBlocks::RESIN_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::RESIN_BRICK_STAIRS);
}

Block VanillaBlocks::CINNABAR_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_BRICK_STAIRS);
}

Block VanillaBlocks::DEEPSLATE_GOLD_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_GOLD_ORE);
}

Block VanillaBlocks::ANCIENT_DEBRIS() {
    return buildFromTypeId(BlockTypeIds::ANCIENT_DEBRIS);
}

Block VanillaBlocks::VAULT() {
    return buildFromTypeId(BlockTypeIds::VAULT);
}

Block VanillaBlocks::BEEHIVE() {
    return buildFromTypeId(BlockTypeIds::BEEHIVE);
}

Block VanillaBlocks::STRIPPED_POPLAR_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_POPLAR_LOG);
}

Block VanillaBlocks::HARD_ORANGE_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_ORANGE_STAINED_GLASS);
}

Block VanillaBlocks::JUNGLE_DOOR() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_DOOR);
}

Block VanillaBlocks::GLASS() {
    return buildFromTypeId(BlockTypeIds::GLASS);
}

Block VanillaBlocks::WITHER_ROSE() {
    return buildFromTypeId(BlockTypeIds::WITHER_ROSE);
}

Block VanillaBlocks::NETHER_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::STRIPPED_POPLAR_WOOD() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_POPLAR_WOOD);
}

Block VanillaBlocks::EXPOSED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_CUT_COPPER);
}

Block VanillaBlocks::WAXED_WEATHERED_CUT_COPPER_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_CUT_COPPER_STAIRS);
}

Block VanillaBlocks::MANGROVE_ROOTS() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_ROOTS);
}

Block VanillaBlocks::YELLOW_CANDLE() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CANDLE);
}

Block VanillaBlocks::ACACIA_STAIRS() {
    return buildFromTypeId(BlockTypeIds::ACACIA_STAIRS);
}

Block VanillaBlocks::BAMBOO_MOSAIC_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_MOSAIC_STAIRS);
}

Block VanillaBlocks::BROWN_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::BROWN_CONCRETE);
}

Block VanillaBlocks::CHERRY_SLAB() {
    return buildFromTypeId(BlockTypeIds::CHERRY_SLAB);
}

Block VanillaBlocks::CHISELED_RESIN_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CHISELED_RESIN_BRICKS);
}

Block VanillaBlocks::BUBBLE_CORAL() {
    return buildFromTypeId(BlockTypeIds::BUBBLE_CORAL);
}

Block VanillaBlocks::ORANGE_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::ORANGE_SHULKER_BOX);
}

Block VanillaBlocks::LIGHT_GRAY_CANDLE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CANDLE);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_PRESSURE_PLATE);
}

Block VanillaBlocks::ACACIA_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::ACACIA_STANDING_SIGN);
}

Block VanillaBlocks::POLISHED_GRANITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_GRANITE_SLAB);
}

Block VanillaBlocks::CINNABAR() {
    return buildFromTypeId(BlockTypeIds::CINNABAR);
}

Block VanillaBlocks::SMOOTH_RED_SANDSTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_RED_SANDSTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::TUFF_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::TUFF_BRICK_STAIRS);
}

Block VanillaBlocks::BLUE_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::BLUE_SHULKER_BOX);
}

Block VanillaBlocks::EXPOSED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_BULB);
}

Block VanillaBlocks::EXPOSED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_BARS);
}

Block VanillaBlocks::DEAD_FIRE_CORAL() {
    return buildFromTypeId(BlockTypeIds::DEAD_FIRE_CORAL);
}

Block VanillaBlocks::STONE_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::STONE_BRICK_SLAB);
}

Block VanillaBlocks::CRIMSON_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_STAIRS);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_BARS);
}

Block VanillaBlocks::STRIPPED_SPRUCE_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_SPRUCE_LOG);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_BULB);
}

Block VanillaBlocks::POPLAR_PLANKS() {
    return buildFromTypeId(BlockTypeIds::POPLAR_PLANKS);
}

Block VanillaBlocks::PUMPKIN_STEM() {
    return buildFromTypeId(BlockTypeIds::PUMPKIN_STEM);
}

Block VanillaBlocks::AZALEA_LEAVES_FLOWERED() {
    return buildFromTypeId(BlockTypeIds::AZALEA_LEAVES_FLOWERED);
}

Block VanillaBlocks::HARD_MAGENTA_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_MAGENTA_STAINED_GLASS_PANE);
}

Block VanillaBlocks::STICKY_PISTON_ARM_COLLISION() {
    return buildFromTypeId(BlockTypeIds::STICKY_PISTON_ARM_COLLISION);
}

Block VanillaBlocks::CHISELED_CINNABAR() {
    return buildFromTypeId(BlockTypeIds::CHISELED_CINNABAR);
}

Block VanillaBlocks::WARPED_NYLIUM() {
    return buildFromTypeId(BlockTypeIds::WARPED_NYLIUM);
}

Block VanillaBlocks::DEEPSLATE_EMERALD_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_EMERALD_ORE);
}

Block VanillaBlocks::ACACIA_SAPLING() {
    return buildFromTypeId(BlockTypeIds::ACACIA_SAPLING);
}

Block VanillaBlocks::QUARTZ_BRICKS() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_BRICKS);
}

Block VanillaBlocks::ANDESITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ANDESITE_SLAB);
}

Block VanillaBlocks::UNPOWERED_COMPARATOR() {
    return buildFromTypeId(BlockTypeIds::UNPOWERED_COMPARATOR);
}

Block VanillaBlocks::LIME_CANDLE() {
    return buildFromTypeId(BlockTypeIds::LIME_CANDLE);
}

Block VanillaBlocks::STRUCTURE_BLOCK() {
    return buildFromTypeId(BlockTypeIds::STRUCTURE_BLOCK);
}

Block VanillaBlocks::END_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::END_BRICK_STAIRS);
}

Block VanillaBlocks::PURPLE_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::PURPLE_TERRACOTTA);
}

Block VanillaBlocks::TARGET() {
    return buildFromTypeId(BlockTypeIds::TARGET);
}

Block VanillaBlocks::WOODEN_BUTTON() {
    return buildFromTypeId(BlockTypeIds::WOODEN_BUTTON);
}

Block VanillaBlocks::MANGROVE_DOOR() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_DOOR);
}

Block VanillaBlocks::END_STONE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::END_STONE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::HARD_LIME_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_LIME_STAINED_GLASS);
}

Block VanillaBlocks::WEATHERED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_DOOR);
}

Block VanillaBlocks::PEARLESCENT_FROGLIGHT() {
    return buildFromTypeId(BlockTypeIds::PEARLESCENT_FROGLIGHT);
}

Block VanillaBlocks::BAMBOO_BUTTON() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_BUTTON);
}

Block VanillaBlocks::TALL_GRASS() {
    return buildFromTypeId(BlockTypeIds::TALL_GRASS);
}

Block VanillaBlocks::WEATHERED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_LANTERN);
}

Block VanillaBlocks::LIGHT_BLOCK_12() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_12);
}

Block VanillaBlocks::LIGHT_BLOCK_13() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_13);
}

Block VanillaBlocks::LIGHT_BLOCK_10() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_10);
}

Block VanillaBlocks::LIGHT_BLOCK_11() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_11);
}

Block VanillaBlocks::LIGHT_BLOCK_14() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_14);
}

Block VanillaBlocks::LIGHT_BLOCK_15() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLOCK_15);
}

Block VanillaBlocks::NETHER_SPROUTS() {
    return buildFromTypeId(BlockTypeIds::NETHER_SPROUTS);
}

Block VanillaBlocks::CYAN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::CYAN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::DEAD_HORN_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DEAD_HORN_CORAL_BLOCK);
}

Block VanillaBlocks::VERDANT_FROGLIGHT() {
    return buildFromTypeId(BlockTypeIds::VERDANT_FROGLIGHT);
}

Block VanillaBlocks::HARD_GRAY_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_GRAY_STAINED_GLASS_PANE);
}

Block VanillaBlocks::RESIN_BLOCK() {
    return buildFromTypeId(BlockTypeIds::RESIN_BLOCK);
}

Block VanillaBlocks::WARPED_SLAB() {
    return buildFromTypeId(BlockTypeIds::WARPED_SLAB);
}

Block VanillaBlocks::WARPED_STEM() {
    return buildFromTypeId(BlockTypeIds::WARPED_STEM);
}

Block VanillaBlocks::HORN_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::HORN_CORAL_FAN);
}

Block VanillaBlocks::GREEN_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::GREEN_SHULKER_BOX);
}

Block VanillaBlocks::LARGE_FERN() {
    return buildFromTypeId(BlockTypeIds::LARGE_FERN);
}

Block VanillaBlocks::STRIPPED_CRIMSON_HYPHAE() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_CRIMSON_HYPHAE);
}

Block VanillaBlocks::COCOA() {
    return buildFromTypeId(BlockTypeIds::COCOA);
}

Block VanillaBlocks::LEVER() {
    return buildFromTypeId(BlockTypeIds::LEVER);
}

Block VanillaBlocks::CINNABAR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_STAIRS);
}

Block VanillaBlocks::BAMBOO_SLAB() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_SLAB);
}

Block VanillaBlocks::HARD_GREEN_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_GREEN_STAINED_GLASS);
}

Block VanillaBlocks::BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BRICK_STAIRS);
}

Block VanillaBlocks::COLORED_TORCH_GREEN() {
    return buildFromTypeId(BlockTypeIds::COLORED_TORCH_GREEN);
}

Block VanillaBlocks::WEATHERED_COPPER_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_TRAPDOOR);
}

Block VanillaBlocks::SMOOTH_RED_SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_RED_SANDSTONE_SLAB);
}

Block VanillaBlocks::MOSS_BLOCK() {
    return buildFromTypeId(BlockTypeIds::MOSS_BLOCK);
}

Block VanillaBlocks::PURPLE_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CONCRETE_POWDER);
}

Block VanillaBlocks::PINK_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::PINK_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::SHORT_GRASS() {
    return buildFromTypeId(BlockTypeIds::SHORT_GRASS);
}

Block VanillaBlocks::WAXED_WEATHERED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::FIRE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::FIRE_CORAL_FAN);
}

Block VanillaBlocks::SPRUCE_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_TRAPDOOR);
}

Block VanillaBlocks::CHAIN_COMMAND_BLOCK() {
    return buildFromTypeId(BlockTypeIds::CHAIN_COMMAND_BLOCK);
}

Block VanillaBlocks::RED_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::RED_SANDSTONE);
}

Block VanillaBlocks::RED_NETHER_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_NETHER_BRICK_SLAB);
}

Block VanillaBlocks::EXPOSED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_CHISELED_COPPER);
}

Block VanillaBlocks::SPRUCE_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_FENCE_GATE);
}

Block VanillaBlocks::EXPOSED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::RED_NETHER_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::RED_NETHER_BRICK_STAIRS);
}

Block VanillaBlocks::GREEN_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::GREEN_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::JUNGLE_PLANKS() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_PLANKS);
}

Block VanillaBlocks::DEEPSLATE_REDSTONE_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_REDSTONE_ORE);
}

Block VanillaBlocks::DEAD_BRAIN_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DEAD_BRAIN_CORAL_BLOCK);
}

Block VanillaBlocks::MANGROVE_FENCE() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_FENCE);
}

Block VanillaBlocks::OXIDIZED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_GRATE);
}

Block VanillaBlocks::ANVIL() {
    return buildFromTypeId(BlockTypeIds::ANVIL);
}

Block VanillaBlocks::BIRCH_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::BIRCH_TRAPDOOR);
}

Block VanillaBlocks::TUFF_BRICKS() {
    return buildFromTypeId(BlockTypeIds::TUFF_BRICKS);
}

Block VanillaBlocks::MANGROVE_LEAVES() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_LEAVES);
}

Block VanillaBlocks::STRAW_BED() {
    return buildFromTypeId(BlockTypeIds::STRAW_BED);
}

Block VanillaBlocks::COBBLED_DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::COBBLED_DEEPSLATE);
}

Block VanillaBlocks::POPLAR_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POPLAR_DOUBLE_SLAB);
}

Block VanillaBlocks::QUARTZ_SLAB() {
    return buildFromTypeId(BlockTypeIds::QUARTZ_SLAB);
}

Block VanillaBlocks::BOOKSHELF() {
    return buildFromTypeId(BlockTypeIds::BOOKSHELF);
}

Block VanillaBlocks::MUD() {
    return buildFromTypeId(BlockTypeIds::MUD);
}

Block VanillaBlocks::LIT_PUMPKIN() {
    return buildFromTypeId(BlockTypeIds::LIT_PUMPKIN);
}

Block VanillaBlocks::ICE() {
    return buildFromTypeId(BlockTypeIds::ICE);
}

Block VanillaBlocks::AIR() {
    return buildFromTypeId(BlockTypeIds::AIR);
}

Block VanillaBlocks::BED() {
    return buildFromTypeId(BlockTypeIds::BED);
}

Block VanillaBlocks::BLACK_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::BLACK_CONCRETE);
}

Block VanillaBlocks::TNT() {
    return buildFromTypeId(BlockTypeIds::TNT);
}

Block VanillaBlocks::PURPLE_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CANDLE_CAKE);
}

Block VanillaBlocks::WEB() {
    return buildFromTypeId(BlockTypeIds::WEB);
}

Block VanillaBlocks::DEAD_TUBE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_TUBE_CORAL_FAN);
}

Block VanillaBlocks::OXIDIZED_COPPER_CHEST() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_CHEST);
}

Block VanillaBlocks::OXIDIZED_COPPER_CHAIN() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_CHAIN);
}

Block VanillaBlocks::PALE_OAK_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_STANDING_SIGN);
}

Block VanillaBlocks::POLISHED_DIORITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DIORITE_STAIRS);
}

Block VanillaBlocks::BLUE_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::BLUE_CONCRETE_POWDER);
}

Block VanillaBlocks::ORANGE_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CONCRETE);
}

Block VanillaBlocks::CRYING_OBSIDIAN() {
    return buildFromTypeId(BlockTypeIds::CRYING_OBSIDIAN);
}

Block VanillaBlocks::LIME_CARPET() {
    return buildFromTypeId(BlockTypeIds::LIME_CARPET);
}

Block VanillaBlocks::CLOSED_EYEBLOSSOM() {
    return buildFromTypeId(BlockTypeIds::CLOSED_EYEBLOSSOM);
}

Block VanillaBlocks::DEAD_FIRE_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_FIRE_CORAL_FAN);
}

Block VanillaBlocks::DECORATED_POT() {
    return buildFromTypeId(BlockTypeIds::DECORATED_POT);
}

Block VanillaBlocks::GRANITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRANITE_DOUBLE_SLAB);
}

Block VanillaBlocks::ENCHANTING_TABLE() {
    return buildFromTypeId(BlockTypeIds::ENCHANTING_TABLE);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_WALL);
}

Block VanillaBlocks::WAXED_EXPOSED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::BUBBLE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::BUBBLE_CORAL_WALL_FAN);
}

Block VanillaBlocks::ORANGE_TULIP() {
    return buildFromTypeId(BlockTypeIds::ORANGE_TULIP);
}

Block VanillaBlocks::BROWN_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::BROWN_SHULKER_BOX);
}

Block VanillaBlocks::POPLAR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POPLAR_STAIRS);
}

Block VanillaBlocks::AZALEA() {
    return buildFromTypeId(BlockTypeIds::AZALEA);
}

Block VanillaBlocks::MUD_BRICKS() {
    return buildFromTypeId(BlockTypeIds::MUD_BRICKS);
}

Block VanillaBlocks::BIRCH_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::BIRCH_WALL_SIGN);
}

Block VanillaBlocks::BAMBOO_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_WALL_SIGN);
}

Block VanillaBlocks::ACACIA_WOOD() {
    return buildFromTypeId(BlockTypeIds::ACACIA_WOOD);
}

Block VanillaBlocks::SULFUR_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::SULFUR_BRICK_WALL);
}

Block VanillaBlocks::GRAY_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::GRAY_STAINED_GLASS_PANE);
}

Block VanillaBlocks::HOPPER() {
    return buildFromTypeId(BlockTypeIds::HOPPER);
}

Block VanillaBlocks::HARD_RED_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_RED_STAINED_GLASS);
}

Block VanillaBlocks::BELL() {
    return buildFromTypeId(BlockTypeIds::BELL);
}

Block VanillaBlocks::LECTERN() {
    return buildFromTypeId(BlockTypeIds::LECTERN);
}

Block VanillaBlocks::BUSH() {
    return buildFromTypeId(BlockTypeIds::BUSH);
}

Block VanillaBlocks::STRIPPED_CRIMSON_STEM() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_CRIMSON_STEM);
}

Block VanillaBlocks::STANDING_BANNER() {
    return buildFromTypeId(BlockTypeIds::STANDING_BANNER);
}

Block VanillaBlocks::LIGHT_BLUE_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_SHULKER_BOX);
}

Block VanillaBlocks::JUNGLE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_STAIRS);
}

Block VanillaBlocks::MANGROVE_PROPAGULE() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_PROPAGULE);
}

Block VanillaBlocks::CACTUS() {
    return buildFromTypeId(BlockTypeIds::CACTUS);
}

Block VanillaBlocks::BUDDING_AMETHYST() {
    return buildFromTypeId(BlockTypeIds::BUDDING_AMETHYST);
}

Block VanillaBlocks::SNIFFER_EGG() {
    return buildFromTypeId(BlockTypeIds::SNIFFER_EGG);
}

Block VanillaBlocks::POLISHED_DIORITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DIORITE_DOUBLE_SLAB);
}

Block VanillaBlocks::BIRCH_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BIRCH_STAIRS);
}

Block VanillaBlocks::NETHER_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK_WALL);
}

Block VanillaBlocks::PURPLE_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::PURPLE_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::GREEN_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::GREEN_CONCRETE_POWDER);
}

Block VanillaBlocks::BEDROCK() {
    return buildFromTypeId(BlockTypeIds::BEDROCK);
}

Block VanillaBlocks::SPRUCE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_SLAB);
}

Block VanillaBlocks::BLACKSTONE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BLACKSTONE_STAIRS);
}

Block VanillaBlocks::BLUE_ICE() {
    return buildFromTypeId(BlockTypeIds::BLUE_ICE);
}

Block VanillaBlocks::CYAN_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::CYAN_SHULKER_BOX);
}

Block VanillaBlocks::HARD_RED_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_RED_STAINED_GLASS_PANE);
}

Block VanillaBlocks::POLISHED_ANDESITE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_ANDESITE_STAIRS);
}

Block VanillaBlocks::DEAD_HORN_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_HORN_CORAL_WALL_FAN);
}

Block VanillaBlocks::PIGLIN_HEAD() {
    return buildFromTypeId(BlockTypeIds::PIGLIN_HEAD);
}

Block VanillaBlocks::SCULK() {
    return buildFromTypeId(BlockTypeIds::SCULK);
}

Block VanillaBlocks::HARD_PURPLE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_PURPLE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::NETHERRACK() {
    return buildFromTypeId(BlockTypeIds::NETHERRACK);
}

Block VanillaBlocks::PURPLE_CANDLE() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CANDLE);
}

Block VanillaBlocks::SPRUCE_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_STANDING_SIGN);
}

Block VanillaBlocks::MANGROVE_BUTTON() {
    return buildFromTypeId(BlockTypeIds::MANGROVE_BUTTON);
}

Block VanillaBlocks::ORANGE_CARPET() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CARPET);
}

Block VanillaBlocks::DEAD_HORN_CORAL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_HORN_CORAL_FAN);
}

Block VanillaBlocks::LANTERN() {
    return buildFromTypeId(BlockTypeIds::LANTERN);
}

Block VanillaBlocks::CRIMSON_SHELF() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_SHELF);
}

Block VanillaBlocks::WAXED_WEATHERED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_COPPER_DOOR);
}

Block VanillaBlocks::RED_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::RED_STAINED_GLASS_PANE);
}

Block VanillaBlocks::LIT_BLAST_FURNACE() {
    return buildFromTypeId(BlockTypeIds::LIT_BLAST_FURNACE);
}

Block VanillaBlocks::WAXED_OXIDIZED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_LIGHTNING_ROD);
}

Block VanillaBlocks::CINNABAR_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_BRICK_SLAB);
}

Block VanillaBlocks::PINK_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::PINK_STAINED_GLASS_PANE);
}

Block VanillaBlocks::LIGHT_BLUE_WOOL() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_WOOL);
}

Block VanillaBlocks::ALLOW() {
    return buildFromTypeId(BlockTypeIds::ALLOW);
}

Block VanillaBlocks::DARK_OAK_FENCE() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_FENCE);
}

Block VanillaBlocks::DEPRECATED_PURPUR_BLOCK_2() {
    return buildFromTypeId(BlockTypeIds::DEPRECATED_PURPUR_BLOCK_2);
}

Block VanillaBlocks::DEPRECATED_PURPUR_BLOCK_1() {
    return buildFromTypeId(BlockTypeIds::DEPRECATED_PURPUR_BLOCK_1);
}

Block VanillaBlocks::BIRCH_DOOR() {
    return buildFromTypeId(BlockTypeIds::BIRCH_DOOR);
}

Block VanillaBlocks::CHERRY_SHELF() {
    return buildFromTypeId(BlockTypeIds::CHERRY_SHELF);
}

Block VanillaBlocks::CHEST() {
    return buildFromTypeId(BlockTypeIds::CHEST);
}

Block VanillaBlocks::CHERRY_WOOD() {
    return buildFromTypeId(BlockTypeIds::CHERRY_WOOD);
}

Block VanillaBlocks::CLAY() {
    return buildFromTypeId(BlockTypeIds::CLAY);
}

Block VanillaBlocks::CHERRY_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CHERRY_STAIRS);
}

Block VanillaBlocks::CAKE() {
    return buildFromTypeId(BlockTypeIds::CAKE);
}

Block VanillaBlocks::CRIMSON_HANGING_SIGN() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_HANGING_SIGN);
}

Block VanillaBlocks::SCULK_VEIN() {
    return buildFromTypeId(BlockTypeIds::SCULK_VEIN);
}

Block VanillaBlocks::DEAD_BRAIN_CORAL() {
    return buildFromTypeId(BlockTypeIds::DEAD_BRAIN_CORAL);
}

Block VanillaBlocks::DEEPSLATE_COAL_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_COAL_ORE);
}

Block VanillaBlocks::WEATHERED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_CUT_COPPER);
}

Block VanillaBlocks::WARPED_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::WARPED_STANDING_SIGN);
}

Block VanillaBlocks::POLISHED_ANDESITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_ANDESITE_DOUBLE_SLAB);
}

Block VanillaBlocks::POLISHED_CINNABAR() {
    return buildFromTypeId(BlockTypeIds::POLISHED_CINNABAR);
}

Block VanillaBlocks::CRACKED_POLISHED_BLACKSTONE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CRACKED_POLISHED_BLACKSTONE_BRICKS);
}

Block VanillaBlocks::BAMBOO_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_STANDING_SIGN);
}

Block VanillaBlocks::FLOWING_LAVA() {
    return buildFromTypeId(BlockTypeIds::FLOWING_LAVA);
}

Block VanillaBlocks::WITHER_SKELETON_SKULL() {
    return buildFromTypeId(BlockTypeIds::WITHER_SKELETON_SKULL);
}

Block VanillaBlocks::POLISHED_TUFF() {
    return buildFromTypeId(BlockTypeIds::POLISHED_TUFF);
}

Block VanillaBlocks::MAGENTA_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_STAINED_GLASS);
}

Block VanillaBlocks::HARD_WHITE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_WHITE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::ACACIA_BUTTON() {
    return buildFromTypeId(BlockTypeIds::ACACIA_BUTTON);
}

Block VanillaBlocks::HARD_CYAN_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_CYAN_STAINED_GLASS_PANE);
}

Block VanillaBlocks::LIT_FURNACE() {
    return buildFromTypeId(BlockTypeIds::LIT_FURNACE);
}

Block VanillaBlocks::CHISELED_NETHER_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CHISELED_NETHER_BRICKS);
}

Block VanillaBlocks::WARPED_BUTTON() {
    return buildFromTypeId(BlockTypeIds::WARPED_BUTTON);
}

Block VanillaBlocks::RED_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::RED_CONCRETE_POWDER);
}

Block VanillaBlocks::LIGHT_GRAY_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CONCRETE_POWDER);
}

Block VanillaBlocks::DEEPSLATE_LAPIS_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_LAPIS_ORE);
}

Block VanillaBlocks::DEAD_BUBBLE_CORAL() {
    return buildFromTypeId(BlockTypeIds::DEAD_BUBBLE_CORAL);
}

Block VanillaBlocks::CHERRY_SAPLING() {
    return buildFromTypeId(BlockTypeIds::CHERRY_SAPLING);
}

Block VanillaBlocks::CHERRY_LOG() {
    return buildFromTypeId(BlockTypeIds::CHERRY_LOG);
}

Block VanillaBlocks::PRISMARINE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_STAIRS);
}

Block VanillaBlocks::WHITE_CARPET() {
    return buildFromTypeId(BlockTypeIds::WHITE_CARPET);
}

Block VanillaBlocks::POLISHED_SULFUR_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_SULFUR_SLAB);
}

Block VanillaBlocks::CYAN_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::CYAN_CONCRETE);
}

Block VanillaBlocks::POLISHED_TUFF_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_TUFF_STAIRS);
}

Block VanillaBlocks::DRAGON_EGG() {
    return buildFromTypeId(BlockTypeIds::DRAGON_EGG);
}

Block VanillaBlocks::BLUE_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::BLUE_CONCRETE);
}

Block VanillaBlocks::NETHER_BRICK() {
    return buildFromTypeId(BlockTypeIds::NETHER_BRICK);
}

Block VanillaBlocks::DEEPSLATE_IRON_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_IRON_ORE);
}

Block VanillaBlocks::ELEMENT_1() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_1);
}

Block VanillaBlocks::ELEMENT_0() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_0);
}

Block VanillaBlocks::ELEMENT_3() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_3);
}

Block VanillaBlocks::ELEMENT_2() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_2);
}

Block VanillaBlocks::ELEMENT_5() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_5);
}

Block VanillaBlocks::ELEMENT_4() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_4);
}

Block VanillaBlocks::ELEMENT_7() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_7);
}

Block VanillaBlocks::ELEMENT_6() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_6);
}

Block VanillaBlocks::ELEMENT_9() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_9);
}

Block VanillaBlocks::ELEMENT_8() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_8);
}

Block VanillaBlocks::OXEYE_DAISY() {
    return buildFromTypeId(BlockTypeIds::OXEYE_DAISY);
}

Block VanillaBlocks::CAMERA() {
    return buildFromTypeId(BlockTypeIds::CAMERA);
}

Block VanillaBlocks::WHEAT() {
    return buildFromTypeId(BlockTypeIds::WHEAT);
}

Block VanillaBlocks::WAXED_CUT_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_CUT_COPPER);
}

Block VanillaBlocks::SULFUR() {
    return buildFromTypeId(BlockTypeIds::SULFUR);
}

Block VanillaBlocks::IRON_CHAIN() {
    return buildFromTypeId(BlockTypeIds::IRON_CHAIN);
}

Block VanillaBlocks::RESIN_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::RESIN_BRICK_SLAB);
}

Block VanillaBlocks::HEAVY_CORE() {
    return buildFromTypeId(BlockTypeIds::HEAVY_CORE);
}

Block VanillaBlocks::COBBLED_DEEPSLATE_SLAB() {
    return buildFromTypeId(BlockTypeIds::COBBLED_DEEPSLATE_SLAB);
}

Block VanillaBlocks::LILAC() {
    return buildFromTypeId(BlockTypeIds::LILAC);
}

Block VanillaBlocks::PALE_OAK_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_TRAPDOOR);
}

Block VanillaBlocks::CHISELED_QUARTZ_BLOCK() {
    return buildFromTypeId(BlockTypeIds::CHISELED_QUARTZ_BLOCK);
}

Block VanillaBlocks::SPORE_BLOSSOM() {
    return buildFromTypeId(BlockTypeIds::SPORE_BLOSSOM);
}

Block VanillaBlocks::WAXED_EXPOSED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::WAXED_EXPOSED_COPPER_LANTERN);
}

Block VanillaBlocks::CRIMSON_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_STANDING_SIGN);
}

Block VanillaBlocks::DARKOAK_STANDING_SIGN() {
    return buildFromTypeId(BlockTypeIds::DARKOAK_STANDING_SIGN);
}

Block VanillaBlocks::WEATHERED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::PALE_OAK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_STAIRS);
}

Block VanillaBlocks::EMERALD_ORE() {
    return buildFromTypeId(BlockTypeIds::EMERALD_ORE);
}

Block VanillaBlocks::BROWN_MUSHROOM_BLOCK() {
    return buildFromTypeId(BlockTypeIds::BROWN_MUSHROOM_BLOCK);
}

Block VanillaBlocks::GRAY_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::GRAY_CONCRETE_POWDER);
}

Block VanillaBlocks::PETRIFIED_OAK_SLAB() {
    return buildFromTypeId(BlockTypeIds::PETRIFIED_OAK_SLAB);
}

Block VanillaBlocks::GRAY_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::GRAY_CONCRETE);
}

Block VanillaBlocks::PINK_CANDLE() {
    return buildFromTypeId(BlockTypeIds::PINK_CANDLE);
}

Block VanillaBlocks::RED_NETHER_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::RED_NETHER_BRICK_WALL);
}

Block VanillaBlocks::PURPLE_SHULKER_BOX() {
    return buildFromTypeId(BlockTypeIds::PURPLE_SHULKER_BOX);
}

Block VanillaBlocks::CARVED_PUMPKIN() {
    return buildFromTypeId(BlockTypeIds::CARVED_PUMPKIN);
}

Block VanillaBlocks::DROPPER() {
    return buildFromTypeId(BlockTypeIds::DROPPER);
}

Block VanillaBlocks::SPRUCE_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_WALL_SIGN);
}

Block VanillaBlocks::STRIPPED_WARPED_STEM() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_WARPED_STEM);
}

Block VanillaBlocks::CANDLE() {
    return buildFromTypeId(BlockTypeIds::CANDLE);
}

Block VanillaBlocks::POLISHED_ANDESITE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_ANDESITE_SLAB);
}

Block VanillaBlocks::POINTED_DRIPSTONE() {
    return buildFromTypeId(BlockTypeIds::POINTED_DRIPSTONE);
}

Block VanillaBlocks::RED_CARPET() {
    return buildFromTypeId(BlockTypeIds::RED_CARPET);
}

Block VanillaBlocks::NETHERREACTOR() {
    return buildFromTypeId(BlockTypeIds::NETHERREACTOR);
}

Block VanillaBlocks::CUT_RED_SANDSTONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CUT_RED_SANDSTONE_SLAB);
}

Block VanillaBlocks::DEEPSLATE_BRICK_STAIRS() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_BRICK_STAIRS);
}

Block VanillaBlocks::POPLAR_FENCE() {
    return buildFromTypeId(BlockTypeIds::POPLAR_FENCE);
}

Block VanillaBlocks::DARK_PRISMARINE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::DARK_PRISMARINE_STAIRS);
}

Block VanillaBlocks::CREAKING_HEART() {
    return buildFromTypeId(BlockTypeIds::CREAKING_HEART);
}

Block VanillaBlocks::PALE_OAK_BUTTON() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_BUTTON);
}

Block VanillaBlocks::CHISELED_TUFF_BRICKS() {
    return buildFromTypeId(BlockTypeIds::CHISELED_TUFF_BRICKS);
}

Block VanillaBlocks::LIGHT_BLUE_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CONCRETE);
}

Block VanillaBlocks::EXPOSED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::EXPOSED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::ORANGE_POPLAR_LEAVES() {
    return buildFromTypeId(BlockTypeIds::ORANGE_POPLAR_LEAVES);
}

Block VanillaBlocks::RED_TULIP() {
    return buildFromTypeId(BlockTypeIds::RED_TULIP);
}

Block VanillaBlocks::CHEMICAL_HEAT() {
    return buildFromTypeId(BlockTypeIds::CHEMICAL_HEAT);
}

Block VanillaBlocks::TRIP_WIRE() {
    return buildFromTypeId(BlockTypeIds::TRIP_WIRE);
}

Block VanillaBlocks::POPLAR_SAPLING() {
    return buildFromTypeId(BlockTypeIds::POPLAR_SAPLING);
}

Block VanillaBlocks::CAULDRON() {
    return buildFromTypeId(BlockTypeIds::CAULDRON);
}

Block VanillaBlocks::CAVE_VINES_HEAD_WITH_BERRIES() {
    return buildFromTypeId(BlockTypeIds::CAVE_VINES_HEAD_WITH_BERRIES);
}

Block VanillaBlocks::TUBE_CORAL_BLOCK() {
    return buildFromTypeId(BlockTypeIds::TUBE_CORAL_BLOCK);
}

Block VanillaBlocks::CHISELED_RED_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::CHISELED_RED_SANDSTONE);
}

Block VanillaBlocks::DEAD_TUBE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::DEAD_TUBE_CORAL_WALL_FAN);
}

Block VanillaBlocks::BIRCH_SAPLING() {
    return buildFromTypeId(BlockTypeIds::BIRCH_SAPLING);
}

Block VanillaBlocks::DARK_OAK_TRAPDOOR() {
    return buildFromTypeId(BlockTypeIds::DARK_OAK_TRAPDOOR);
}

Block VanillaBlocks::HARD_PINK_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_PINK_STAINED_GLASS_PANE);
}

Block VanillaBlocks::ORANGE_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::ORANGE_TERRACOTTA);
}

Block VanillaBlocks::BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::BRICK_SLAB);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER);
}

Block VanillaBlocks::OAK_PLANKS() {
    return buildFromTypeId(BlockTypeIds::OAK_PLANKS);
}

Block VanillaBlocks::STRIPPED_OAK_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_OAK_LOG);
}

Block VanillaBlocks::SMOOTH_STONE_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_STONE_SLAB);
}

Block VanillaBlocks::POLISHED_ANDESITE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_ANDESITE);
}

Block VanillaBlocks::SEA_LANTERN() {
    return buildFromTypeId(BlockTypeIds::SEA_LANTERN);
}

Block VanillaBlocks::BREWING_STAND() {
    return buildFromTypeId(BlockTypeIds::BREWING_STAND);
}

Block VanillaBlocks::BAMBOO_SAPLING() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_SAPLING);
}

Block VanillaBlocks::WEATHERED_COPPER_BULB() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_BULB);
}

Block VanillaBlocks::WEATHERED_COPPER_BARS() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER_BARS);
}

Block VanillaBlocks::BLAST_FURNACE() {
    return buildFromTypeId(BlockTypeIds::BLAST_FURNACE);
}

Block VanillaBlocks::CRIMSON_ROOTS() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_ROOTS);
}

Block VanillaBlocks::ACACIA_SLAB() {
    return buildFromTypeId(BlockTypeIds::ACACIA_SLAB);
}

Block VanillaBlocks::STONECUTTER_BLOCK() {
    return buildFromTypeId(BlockTypeIds::STONECUTTER_BLOCK);
}

Block VanillaBlocks::POPLAR_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::POPLAR_WALL_SIGN);
}

Block VanillaBlocks::SMOOTH_QUARTZ_SLAB() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_QUARTZ_SLAB);
}

Block VanillaBlocks::SULFUR_BRICK_SLAB() {
    return buildFromTypeId(BlockTypeIds::SULFUR_BRICK_SLAB);
}

Block VanillaBlocks::YELLOW_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CONCRETE_POWDER);
}

Block VanillaBlocks::WHITE_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::WHITE_CANDLE_CAKE);
}

Block VanillaBlocks::CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::CANDLE_CAKE);
}

Block VanillaBlocks::LIME_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::LIME_STAINED_GLASS_PANE);
}

Block VanillaBlocks::END_PORTAL() {
    return buildFromTypeId(BlockTypeIds::END_PORTAL);
}

Block VanillaBlocks::YELLOW_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::YELLOW_STAINED_GLASS);
}

Block VanillaBlocks::POLISHED_CINNABAR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::POLISHED_CINNABAR_STAIRS);
}

Block VanillaBlocks::JUNGLE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::JUNGLE_DOUBLE_SLAB);
}

Block VanillaBlocks::POLISHED_GRANITE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::POLISHED_GRANITE_DOUBLE_SLAB);
}

Block VanillaBlocks::SPRUCE_WOOD() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_WOOD);
}

Block VanillaBlocks::BLACKSTONE() {
    return buildFromTypeId(BlockTypeIds::BLACKSTONE);
}

Block VanillaBlocks::ACACIA_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::ACACIA_FENCE_GATE);
}

Block VanillaBlocks::LIT_DEEPSLATE_REDSTONE_ORE() {
    return buildFromTypeId(BlockTypeIds::LIT_DEEPSLATE_REDSTONE_ORE);
}

Block VanillaBlocks::WILDFLOWERS() {
    return buildFromTypeId(BlockTypeIds::WILDFLOWERS);
}

Block VanillaBlocks::ELEMENT_10() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_10);
}

Block VanillaBlocks::ELEMENT_11() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_11);
}

Block VanillaBlocks::ELEMENT_12() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_12);
}

Block VanillaBlocks::ELEMENT_13() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_13);
}

Block VanillaBlocks::ELEMENT_14() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_14);
}

Block VanillaBlocks::ELEMENT_15() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_15);
}

Block VanillaBlocks::ELEMENT_16() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_16);
}

Block VanillaBlocks::ELEMENT_17() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_17);
}

Block VanillaBlocks::ELEMENT_18() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_18);
}

Block VanillaBlocks::ELEMENT_19() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_19);
}

Block VanillaBlocks::ELEMENT_36() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_36);
}

Block VanillaBlocks::ELEMENT_37() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_37);
}

Block VanillaBlocks::ELEMENT_34() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_34);
}

Block VanillaBlocks::ELEMENT_35() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_35);
}

Block VanillaBlocks::ELEMENT_32() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_32);
}

Block VanillaBlocks::ELEMENT_33() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_33);
}

Block VanillaBlocks::ELEMENT_30() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_30);
}

Block VanillaBlocks::ELEMENT_31() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_31);
}

Block VanillaBlocks::ELEMENT_38() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_38);
}

Block VanillaBlocks::ELEMENT_39() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_39);
}

Block VanillaBlocks::ELEMENT_29() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_29);
}

Block VanillaBlocks::ELEMENT_28() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_28);
}

Block VanillaBlocks::ELEMENT_21() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_21);
}

Block VanillaBlocks::ELEMENT_20() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_20);
}

Block VanillaBlocks::ELEMENT_23() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_23);
}

Block VanillaBlocks::ELEMENT_22() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_22);
}

Block VanillaBlocks::ELEMENT_25() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_25);
}

Block VanillaBlocks::ELEMENT_24() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_24);
}

Block VanillaBlocks::ELEMENT_27() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_27);
}

Block VanillaBlocks::ELEMENT_26() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_26);
}

Block VanillaBlocks::ELEMENT_58() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_58);
}

Block VanillaBlocks::ELEMENT_59() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_59);
}

Block VanillaBlocks::ELEMENT_54() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_54);
}

Block VanillaBlocks::ELEMENT_55() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_55);
}

Block VanillaBlocks::ELEMENT_56() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_56);
}

Block VanillaBlocks::ELEMENT_57() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_57);
}

Block VanillaBlocks::ELEMENT_50() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_50);
}

Block VanillaBlocks::ELEMENT_51() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_51);
}

Block VanillaBlocks::ELEMENT_52() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_52);
}

Block VanillaBlocks::ELEMENT_53() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_53);
}

Block VanillaBlocks::ELEMENT_49() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_49);
}

Block VanillaBlocks::ELEMENT_48() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_48);
}

Block VanillaBlocks::ELEMENT_47() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_47);
}

Block VanillaBlocks::ELEMENT_46() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_46);
}

Block VanillaBlocks::ELEMENT_45() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_45);
}

Block VanillaBlocks::ELEMENT_44() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_44);
}

Block VanillaBlocks::ELEMENT_43() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_43);
}

Block VanillaBlocks::ELEMENT_42() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_42);
}

Block VanillaBlocks::ELEMENT_41() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_41);
}

Block VanillaBlocks::ELEMENT_40() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_40);
}

Block VanillaBlocks::ELEMENT_72() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_72);
}

Block VanillaBlocks::ELEMENT_73() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_73);
}

Block VanillaBlocks::ELEMENT_70() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_70);
}

Block VanillaBlocks::ELEMENT_71() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_71);
}

Block VanillaBlocks::ELEMENT_76() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_76);
}

Block VanillaBlocks::ELEMENT_77() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_77);
}

Block VanillaBlocks::ELEMENT_74() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_74);
}

Block VanillaBlocks::ELEMENT_75() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_75);
}

Block VanillaBlocks::ELEMENT_78() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_78);
}

Block VanillaBlocks::ELEMENT_79() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_79);
}

Block VanillaBlocks::ELEMENT_65() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_65);
}

Block VanillaBlocks::ELEMENT_64() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_64);
}

Block VanillaBlocks::ELEMENT_67() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_67);
}

Block VanillaBlocks::ELEMENT_66() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_66);
}

Block VanillaBlocks::ELEMENT_61() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_61);
}

Block VanillaBlocks::ELEMENT_60() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_60);
}

Block VanillaBlocks::ELEMENT_63() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_63);
}

Block VanillaBlocks::ELEMENT_62() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_62);
}

Block VanillaBlocks::ELEMENT_69() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_69);
}

Block VanillaBlocks::ELEMENT_68() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_68);
}

Block VanillaBlocks::ELEMENT_98() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_98);
}

Block VanillaBlocks::ELEMENT_99() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_99);
}

Block VanillaBlocks::ELEMENT_90() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_90);
}

Block VanillaBlocks::ELEMENT_91() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_91);
}

Block VanillaBlocks::ELEMENT_92() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_92);
}

Block VanillaBlocks::ELEMENT_93() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_93);
}

Block VanillaBlocks::ELEMENT_94() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_94);
}

Block VanillaBlocks::ELEMENT_95() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_95);
}

Block VanillaBlocks::ELEMENT_96() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_96);
}

Block VanillaBlocks::ELEMENT_97() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_97);
}

Block VanillaBlocks::ELEMENT_89() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_89);
}

Block VanillaBlocks::ELEMENT_88() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_88);
}

Block VanillaBlocks::ELEMENT_83() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_83);
}

Block VanillaBlocks::ELEMENT_82() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_82);
}

Block VanillaBlocks::ELEMENT_81() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_81);
}

Block VanillaBlocks::ELEMENT_80() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_80);
}

Block VanillaBlocks::ELEMENT_87() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_87);
}

Block VanillaBlocks::ELEMENT_86() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_86);
}

Block VanillaBlocks::ELEMENT_85() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_85);
}

Block VanillaBlocks::ELEMENT_84() {
    return buildFromTypeId(BlockTypeIds::ELEMENT_84);
}

Block VanillaBlocks::LIT_SMOKER() {
    return buildFromTypeId(BlockTypeIds::LIT_SMOKER);
}

Block VanillaBlocks::LAPIS_ORE() {
    return buildFromTypeId(BlockTypeIds::LAPIS_ORE);
}

Block VanillaBlocks::RED_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::RED_CONCRETE);
}

Block VanillaBlocks::PINK_CARPET() {
    return buildFromTypeId(BlockTypeIds::PINK_CARPET);
}

Block VanillaBlocks::SMOOTH_QUARTZ_STAIRS() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_QUARTZ_STAIRS);
}

Block VanillaBlocks::RED_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::RED_CANDLE_CAKE);
}

Block VanillaBlocks::WAXED_COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_LANTERN);
}

Block VanillaBlocks::AZALEA_LEAVES() {
    return buildFromTypeId(BlockTypeIds::AZALEA_LEAVES);
}

Block VanillaBlocks::PURPUR_BLOCK() {
    return buildFromTypeId(BlockTypeIds::PURPUR_BLOCK);
}

Block VanillaBlocks::CHERRY_WALL_SIGN() {
    return buildFromTypeId(BlockTypeIds::CHERRY_WALL_SIGN);
}

Block VanillaBlocks::CYAN_CANDLE() {
    return buildFromTypeId(BlockTypeIds::CYAN_CANDLE);
}

Block VanillaBlocks::WAXED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER);
}

Block VanillaBlocks::REPEATING_COMMAND_BLOCK() {
    return buildFromTypeId(BlockTypeIds::REPEATING_COMMAND_BLOCK);
}

Block VanillaBlocks::COLORED_TORCH_PURPLE() {
    return buildFromTypeId(BlockTypeIds::COLORED_TORCH_PURPLE);
}

Block VanillaBlocks::NETHER_WART() {
    return buildFromTypeId(BlockTypeIds::NETHER_WART);
}

Block VanillaBlocks::YELLOW_POPLAR_LEAVES() {
    return buildFromTypeId(BlockTypeIds::YELLOW_POPLAR_LEAVES);
}

Block VanillaBlocks::PURPLE_CARPET() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CARPET);
}

Block VanillaBlocks::RED_POPLAR_LEAVES() {
    return buildFromTypeId(BlockTypeIds::RED_POPLAR_LEAVES);
}

Block VanillaBlocks::WAXED_OXIDIZED_DOUBLE_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_DOUBLE_CUT_COPPER_SLAB);
}

Block VanillaBlocks::CRIMSON_FUNGUS() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_FUNGUS);
}

Block VanillaBlocks::CHERRY_PLANKS() {
    return buildFromTypeId(BlockTypeIds::CHERRY_PLANKS);
}

Block VanillaBlocks::POLISHED_DEEPSLATE() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DEEPSLATE);
}

Block VanillaBlocks::TUFF_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::TUFF_DOUBLE_SLAB);
}

Block VanillaBlocks::SMOOTH_RED_SANDSTONE() {
    return buildFromTypeId(BlockTypeIds::SMOOTH_RED_SANDSTONE);
}

Block VanillaBlocks::PURPUR_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PURPUR_STAIRS);
}

Block VanillaBlocks::TUBE_CORAL() {
    return buildFromTypeId(BlockTypeIds::TUBE_CORAL);
}

Block VanillaBlocks::WAXED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_DOOR);
}

Block VanillaBlocks::PORTAL() {
    return buildFromTypeId(BlockTypeIds::PORTAL);
}

Block VanillaBlocks::CINNABAR_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_BRICK_WALL);
}

Block VanillaBlocks::BIRCH_BUTTON() {
    return buildFromTypeId(BlockTypeIds::BIRCH_BUTTON);
}

Block VanillaBlocks::PEONY() {
    return buildFromTypeId(BlockTypeIds::PEONY);
}

Block VanillaBlocks::COMMAND_BLOCK() {
    return buildFromTypeId(BlockTypeIds::COMMAND_BLOCK);
}

Block VanillaBlocks::POLISHED_BLACKSTONE_BUTTON() {
    return buildFromTypeId(BlockTypeIds::POLISHED_BLACKSTONE_BUTTON);
}

Block VanillaBlocks::CRAFTER() {
    return buildFromTypeId(BlockTypeIds::CRAFTER);
}

Block VanillaBlocks::SPRUCE_PLANKS() {
    return buildFromTypeId(BlockTypeIds::SPRUCE_PLANKS);
}

Block VanillaBlocks::MOSSY_COBBLESTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MOSSY_COBBLESTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::FURNACE() {
    return buildFromTypeId(BlockTypeIds::FURNACE);
}

Block VanillaBlocks::INFO_UPDATE2() {
    return buildFromTypeId(BlockTypeIds::INFO_UPDATE2);
}

Block VanillaBlocks::AMETHYST_CLUSTER() {
    return buildFromTypeId(BlockTypeIds::AMETHYST_CLUSTER);
}

Block VanillaBlocks::WAXED_CHISELED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WAXED_CHISELED_COPPER);
}

Block VanillaBlocks::WAXED_CUT_COPPER_SLAB() {
    return buildFromTypeId(BlockTypeIds::WAXED_CUT_COPPER_SLAB);
}

Block VanillaBlocks::POLISHED_DEEPSLATE_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_DEEPSLATE_WALL);
}

Block VanillaBlocks::PRISMARINE_BRICK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_BRICK_DOUBLE_SLAB);
}

Block VanillaBlocks::DRIED_KELP_BLOCK() {
    return buildFromTypeId(BlockTypeIds::DRIED_KELP_BLOCK);
}

Block VanillaBlocks::POPLAR_LOG() {
    return buildFromTypeId(BlockTypeIds::POPLAR_LOG);
}

Block VanillaBlocks::HARD_LIGHT_GRAY_STAINED_GLASS() {
    return buildFromTypeId(BlockTypeIds::HARD_LIGHT_GRAY_STAINED_GLASS);
}

Block VanillaBlocks::CRIMSON_FENCE() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_FENCE);
}

Block VanillaBlocks::CINNABAR_SLAB() {
    return buildFromTypeId(BlockTypeIds::CINNABAR_SLAB);
}

Block VanillaBlocks::CHISELED_TUFF() {
    return buildFromTypeId(BlockTypeIds::CHISELED_TUFF);
}

Block VanillaBlocks::LIME_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::LIME_CONCRETE_POWDER);
}

Block VanillaBlocks::TURTLE_EGG() {
    return buildFromTypeId(BlockTypeIds::TURTLE_EGG);
}

Block VanillaBlocks::MAGMA() {
    return buildFromTypeId(BlockTypeIds::MAGMA);
}

Block VanillaBlocks::DISPENSER() {
    return buildFromTypeId(BlockTypeIds::DISPENSER);
}

Block VanillaBlocks::BROWN_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::BROWN_TERRACOTTA);
}

Block VanillaBlocks::COBBLESTONE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::COBBLESTONE_DOUBLE_SLAB);
}

Block VanillaBlocks::DEEPSLATE_DIAMOND_ORE() {
    return buildFromTypeId(BlockTypeIds::DEEPSLATE_DIAMOND_ORE);
}

Block VanillaBlocks::GRINDSTONE() {
    return buildFromTypeId(BlockTypeIds::GRINDSTONE);
}

Block VanillaBlocks::WAXED_COPPER_GOLEM_STATUE() {
    return buildFromTypeId(BlockTypeIds::WAXED_COPPER_GOLEM_STATUE);
}

Block VanillaBlocks::LIGHT_GRAY_WOOL() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_WOOL);
}

Block VanillaBlocks::SOUL_CAMPFIRE() {
    return buildFromTypeId(BlockTypeIds::SOUL_CAMPFIRE);
}

Block VanillaBlocks::PRISMARINE_BRICKS() {
    return buildFromTypeId(BlockTypeIds::PRISMARINE_BRICKS);
}

Block VanillaBlocks::POPLAR_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::POPLAR_FENCE_GATE);
}

Block VanillaBlocks::WOODEN_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::WOODEN_PRESSURE_PLATE);
}

Block VanillaBlocks::SANDSTONE_WALL() {
    return buildFromTypeId(BlockTypeIds::SANDSTONE_WALL);
}

Block VanillaBlocks::BIRCH_FENCE() {
    return buildFromTypeId(BlockTypeIds::BIRCH_FENCE);
}

Block VanillaBlocks::LIME_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::LIME_CANDLE_CAKE);
}

Block VanillaBlocks::WAXED_OXIDIZED_COPPER_GRATE() {
    return buildFromTypeId(BlockTypeIds::WAXED_OXIDIZED_COPPER_GRATE);
}

Block VanillaBlocks::DAMAGED_ANVIL() {
    return buildFromTypeId(BlockTypeIds::DAMAGED_ANVIL);
}

Block VanillaBlocks::BIRCH_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BIRCH_DOUBLE_SLAB);
}

Block VanillaBlocks::WHITE_CONCRETE() {
    return buildFromTypeId(BlockTypeIds::WHITE_CONCRETE);
}

Block VanillaBlocks::MATERIAL_REDUCER() {
    return buildFromTypeId(BlockTypeIds::MATERIAL_REDUCER);
}

Block VanillaBlocks::TRIAL_SPAWNER() {
    return buildFromTypeId(BlockTypeIds::TRIAL_SPAWNER);
}

Block VanillaBlocks::ACACIA_FENCE() {
    return buildFromTypeId(BlockTypeIds::ACACIA_FENCE);
}

Block VanillaBlocks::GRASS_PATH() {
    return buildFromTypeId(BlockTypeIds::GRASS_PATH);
}

Block VanillaBlocks::RESIN_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::RESIN_BRICK_WALL);
}

Block VanillaBlocks::COBBLED_DEEPSLATE_WALL() {
    return buildFromTypeId(BlockTypeIds::COBBLED_DEEPSLATE_WALL);
}

Block VanillaBlocks::WAXED_WEATHERED_LIGHTNING_ROD() {
    return buildFromTypeId(BlockTypeIds::WAXED_WEATHERED_LIGHTNING_ROD);
}

Block VanillaBlocks::ORANGE_CONCRETE_POWDER() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CONCRETE_POWDER);
}

Block VanillaBlocks::ORANGE_CANDLE_CAKE() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CANDLE_CAKE);
}

Block VanillaBlocks::WEATHERED_COPPER() {
    return buildFromTypeId(BlockTypeIds::WEATHERED_COPPER);
}

Block VanillaBlocks::MOSSY_STONE_BRICK_WALL() {
    return buildFromTypeId(BlockTypeIds::MOSSY_STONE_BRICK_WALL);
}

Block VanillaBlocks::UNLIT_REDSTONE_TORCH() {
    return buildFromTypeId(BlockTypeIds::UNLIT_REDSTONE_TORCH);
}

Block VanillaBlocks::POLISHED_SULFUR_WALL() {
    return buildFromTypeId(BlockTypeIds::POLISHED_SULFUR_WALL);
}

Block VanillaBlocks::PALE_OAK_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PALE_OAK_DOUBLE_SLAB);
}

Block VanillaBlocks::LIME_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::LIME_TERRACOTTA);
}

Block VanillaBlocks::CHERRY_FENCE_GATE() {
    return buildFromTypeId(BlockTypeIds::CHERRY_FENCE_GATE);
}

Block VanillaBlocks::GRAY_GLAZED_TERRACOTTA() {
    return buildFromTypeId(BlockTypeIds::GRAY_GLAZED_TERRACOTTA);
}

Block VanillaBlocks::COLORED_TORCH_RED() {
    return buildFromTypeId(BlockTypeIds::COLORED_TORCH_RED);
}

Block VanillaBlocks::LODESTONE() {
    return buildFromTypeId(BlockTypeIds::LODESTONE);
}

Block VanillaBlocks::BAMBOO_MOSAIC() {
    return buildFromTypeId(BlockTypeIds::BAMBOO_MOSAIC);
}

Block VanillaBlocks::HARD_BLUE_STAINED_GLASS_PANE() {
    return buildFromTypeId(BlockTypeIds::HARD_BLUE_STAINED_GLASS_PANE);
}

Block VanillaBlocks::RAW_IRON_BLOCK() {
    return buildFromTypeId(BlockTypeIds::RAW_IRON_BLOCK);
}

Block VanillaBlocks::POPLAR_WOOD() {
    return buildFromTypeId(BlockTypeIds::POPLAR_WOOD);
}

Block VanillaBlocks::LIGHT_GRAY_CARPET() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CARPET);
}

Block VanillaBlocks::PURPLE_WOOL() {
    return buildFromTypeId(BlockTypeIds::PURPLE_WOOL);
}

Block VanillaBlocks::IRON_BLOCK() {
    return buildFromTypeId(BlockTypeIds::IRON_BLOCK);
}

Block VanillaBlocks::LADDER() {
    return buildFromTypeId(BlockTypeIds::LADDER);
}

Block VanillaBlocks::CRIMSON_PRESSURE_PLATE() {
    return buildFromTypeId(BlockTypeIds::CRIMSON_PRESSURE_PLATE);
}

Block VanillaBlocks::STRIPPED_MANGROVE_LOG() {
    return buildFromTypeId(BlockTypeIds::STRIPPED_MANGROVE_LOG);
}

Block VanillaBlocks::COPPER_LANTERN() {
    return buildFromTypeId(BlockTypeIds::COPPER_LANTERN);
}

Block VanillaBlocks::GRAVEL() {
    return buildFromTypeId(BlockTypeIds::GRAVEL);
}

Block VanillaBlocks::CARTOGRAPHY_TABLE() {
    return buildFromTypeId(BlockTypeIds::CARTOGRAPHY_TABLE);
}

Block VanillaBlocks::OXIDIZED_COPPER_DOOR() {
    return buildFromTypeId(BlockTypeIds::OXIDIZED_COPPER_DOOR);
}

Block VanillaBlocks::TUBE_CORAL_WALL_FAN() {
    return buildFromTypeId(BlockTypeIds::TUBE_CORAL_WALL_FAN);
}

Block VanillaBlocks::DANDELION() {
    return buildFromTypeId(BlockTypeIds::DANDELION);
}

Block VanillaBlocks::GRASS() {
    return buildFromTypeId(BlockTypeIds::GRASS_BLOCK);
}

Block VanillaBlocks::WHITE_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::WHITE_WOOL_SLAB);
}

Block VanillaBlocks::WHITE_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::WHITE_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::WHITE_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WHITE_WOOL_STAIRS);
}

Block VanillaBlocks::WHITE_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::WHITE_CONCRETE_SLAB);
}

Block VanillaBlocks::WHITE_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::WHITE_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::WHITE_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::WHITE_CONCRETE_STAIRS);
}

Block VanillaBlocks::ORANGE_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::ORANGE_WOOL_SLAB);
}

Block VanillaBlocks::ORANGE_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ORANGE_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::ORANGE_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::ORANGE_WOOL_STAIRS);
}

Block VanillaBlocks::ORANGE_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CONCRETE_SLAB);
}

Block VanillaBlocks::ORANGE_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::ORANGE_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::ORANGE_CONCRETE_STAIRS);
}

Block VanillaBlocks::MAGENTA_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_WOOL_SLAB);
}

Block VanillaBlocks::MAGENTA_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::MAGENTA_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_WOOL_STAIRS);
}

Block VanillaBlocks::MAGENTA_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CONCRETE_SLAB);
}

Block VanillaBlocks::MAGENTA_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::MAGENTA_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::MAGENTA_CONCRETE_STAIRS);
}

Block VanillaBlocks::LIGHT_BLUE_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_WOOL_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_WOOL_STAIRS);
}

Block VanillaBlocks::LIGHT_BLUE_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CONCRETE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_BLUE_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_BLUE_CONCRETE_STAIRS);
}

Block VanillaBlocks::YELLOW_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::YELLOW_WOOL_SLAB);
}

Block VanillaBlocks::YELLOW_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::YELLOW_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::YELLOW_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::YELLOW_WOOL_STAIRS);
}

Block VanillaBlocks::YELLOW_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CONCRETE_SLAB);
}

Block VanillaBlocks::YELLOW_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::YELLOW_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::YELLOW_CONCRETE_STAIRS);
}

Block VanillaBlocks::LIME_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIME_WOOL_SLAB);
}

Block VanillaBlocks::LIME_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIME_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::LIME_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIME_WOOL_STAIRS);
}

Block VanillaBlocks::LIME_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIME_CONCRETE_SLAB);
}

Block VanillaBlocks::LIME_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIME_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIME_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIME_CONCRETE_STAIRS);
}

Block VanillaBlocks::PINK_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::PINK_WOOL_SLAB);
}

Block VanillaBlocks::PINK_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PINK_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::PINK_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PINK_WOOL_STAIRS);
}

Block VanillaBlocks::PINK_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PINK_CONCRETE_SLAB);
}

Block VanillaBlocks::PINK_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PINK_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::PINK_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PINK_CONCRETE_STAIRS);
}

Block VanillaBlocks::GRAY_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRAY_WOOL_SLAB);
}

Block VanillaBlocks::GRAY_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRAY_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::GRAY_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::GRAY_WOOL_STAIRS);
}

Block VanillaBlocks::GRAY_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRAY_CONCRETE_SLAB);
}

Block VanillaBlocks::GRAY_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GRAY_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::GRAY_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::GRAY_CONCRETE_STAIRS);
}

Block VanillaBlocks::LIGHT_GRAY_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_WOOL_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_WOOL_STAIRS);
}

Block VanillaBlocks::LIGHT_GRAY_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CONCRETE_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::LIGHT_GRAY_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::LIGHT_GRAY_CONCRETE_STAIRS);
}

Block VanillaBlocks::CYAN_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::CYAN_WOOL_SLAB);
}

Block VanillaBlocks::CYAN_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CYAN_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::CYAN_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CYAN_WOOL_STAIRS);
}

Block VanillaBlocks::CYAN_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CYAN_CONCRETE_SLAB);
}

Block VanillaBlocks::CYAN_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::CYAN_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::CYAN_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::CYAN_CONCRETE_STAIRS);
}

Block VanillaBlocks::PURPLE_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPLE_WOOL_SLAB);
}

Block VanillaBlocks::PURPLE_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPLE_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::PURPLE_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PURPLE_WOOL_STAIRS);
}

Block VanillaBlocks::PURPLE_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CONCRETE_SLAB);
}

Block VanillaBlocks::PURPLE_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::PURPLE_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::PURPLE_CONCRETE_STAIRS);
}

Block VanillaBlocks::BLUE_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLUE_WOOL_SLAB);
}

Block VanillaBlocks::BLUE_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLUE_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::BLUE_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BLUE_WOOL_STAIRS);
}

Block VanillaBlocks::BLUE_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLUE_CONCRETE_SLAB);
}

Block VanillaBlocks::BLUE_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLUE_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::BLUE_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BLUE_CONCRETE_STAIRS);
}

Block VanillaBlocks::BROWN_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::BROWN_WOOL_SLAB);
}

Block VanillaBlocks::BROWN_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BROWN_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::BROWN_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BROWN_WOOL_STAIRS);
}

Block VanillaBlocks::BROWN_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BROWN_CONCRETE_SLAB);
}

Block VanillaBlocks::BROWN_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BROWN_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::BROWN_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BROWN_CONCRETE_STAIRS);
}

Block VanillaBlocks::GREEN_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::GREEN_WOOL_SLAB);
}

Block VanillaBlocks::GREEN_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GREEN_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::GREEN_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::GREEN_WOOL_STAIRS);
}

Block VanillaBlocks::GREEN_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GREEN_CONCRETE_SLAB);
}

Block VanillaBlocks::GREEN_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::GREEN_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::GREEN_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::GREEN_CONCRETE_STAIRS);
}

Block VanillaBlocks::RED_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_WOOL_SLAB);
}

Block VanillaBlocks::RED_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::RED_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::RED_WOOL_STAIRS);
}

Block VanillaBlocks::RED_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_CONCRETE_SLAB);
}

Block VanillaBlocks::RED_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::RED_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::RED_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::RED_CONCRETE_STAIRS);
}

Block VanillaBlocks::BLACK_WOOL_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACK_WOOL_SLAB);
}

Block VanillaBlocks::BLACK_WOOL_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACK_WOOL_DOUBLE_SLAB);
}

Block VanillaBlocks::BLACK_WOOL_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BLACK_WOOL_STAIRS);
}

Block VanillaBlocks::BLACK_CONCRETE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACK_CONCRETE_SLAB);
}

Block VanillaBlocks::BLACK_CONCRETE_DOUBLE_SLAB() {
    return buildFromTypeId(BlockTypeIds::BLACK_CONCRETE_DOUBLE_SLAB);
}

Block VanillaBlocks::BLACK_CONCRETE_STAIRS() {
    return buildFromTypeId(BlockTypeIds::BLACK_CONCRETE_STAIRS);
}

Block VanillaBlocks::RED_SHRUB() {
    return buildFromTypeId(BlockTypeIds::RED_SHRUB);
}

Block VanillaBlocks::SHELF_MUSHROOM() {
    return buildFromTypeId(BlockTypeIds::SHELF_MUSHROOM);
}

const std::vector<std::unique_ptr<Block>> &VanillaBlocks::getAll() {
    static const std::vector<std::unique_ptr<Block>> blocks = []() {
        std::vector<std::unique_ptr<Block>> result;
        result.reserve(BlockDataTable::getCount() + 1);
        result.push_back(std::make_unique<Block>((int32_t) BlockTypeIds::AIR, "minecraft:air", "Air"));

        for (size_t index = 0; index < BlockDataTable::getCount(); ++index) {
            const BlockData &data = BlockDataTable::getEntries()[index];
            if (data.mTypeId == BlockTypeIds::AIR)
                continue;

            result.push_back(makeBlock(data));
        }

        return result;
    }();

    return blocks;
}

const Block *VanillaBlocks::fromIdentifier(const std::string &identifier) {
    static const std::unordered_map<std::string_view, const Block *> byIdentifier = []() {
        std::unordered_map<std::string_view, const Block *> index;
        index.reserve(getAll().size());
        for (const std::unique_ptr<Block> &block: getAll())
            index.emplace(block->getIdentifier(), block.get());
        return index;
    }();

    const auto match = byIdentifier.find(std::string_view(identifier));
    return match == byIdentifier.end() ? nullptr : match->second;
}

const Block *VanillaBlocks::fromTypeId(int32_t typeId) {
    static const std::unordered_map<int32_t, const Block *> byTypeId = []() {
        std::unordered_map<int32_t, const Block *> index;
        index.reserve(getAll().size());
        for (const std::unique_ptr<Block> &block: getAll())
            index.emplace(block->getTypeId(), block.get());
        return index;
    }();

    const auto match = byTypeId.find(typeId);
    return match == byTypeId.end() ? nullptr : match->second;
}
