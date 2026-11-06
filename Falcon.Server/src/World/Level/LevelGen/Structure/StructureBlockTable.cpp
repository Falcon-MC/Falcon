#include "World/Level/LevelGen/Structure/StructureBlockTable.h"

#include <array>
#include <string>
#include <unordered_map>

namespace {
    // clockwise order, seen from above
    const std::array<const char *, 4> CARDINALS = {"south", "west", "north", "east"};

    int32_t cardinalIndex(const std::string &facing) {
        for (int32_t i = 0; i < 4; ++i) {
            if (facing == CARDINALS[(size_t) i])
                return i;
        }

        return 0;
    }

    std::string property(const StructureTemplate::PaletteEntry &entry, const std::string &name) {
        const auto found = entry.mProperties.find(name);
        return found == entry.mProperties.end() ? std::string() : found->second;
    }

    std::string rotatedFacing(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns) {
        return CARDINALS[(size_t) ((cardinalIndex(property(entry, "facing")) + quarterTurns) & 3)];
    }

    int32_t facingDirection(const std::string &facing) {
        static const std::unordered_map<std::string, int32_t> values = {{"north", 2}, {"south", 3}, {"west", 4},
                                                                        {"east", 5}};
        return values.at(facing);
    }

    int32_t weirdoDirection(const std::string &facing) {
        static const std::unordered_map<std::string, int32_t> values = {{"east", 0}, {"west", 1}, {"south", 2},
                                                                        {"north", 3}};
        return values.at(facing);
    }

    const std::unordered_map<std::string, std::string> &plainBlocks() {
        static const std::unordered_map<std::string, std::string> blocks = {
                {"minecraft:air", "minecraft:air"},
                // data markers: the game replaces them with what they stand for
                {"minecraft:structure_block", "minecraft:air"},
                {"minecraft:snow_block", "minecraft:snow"},
                {"minecraft:packed_ice", "minecraft:packed_ice"},
                {"minecraft:crafting_table", "minecraft:crafting_table"},
                {"minecraft:white_carpet", "minecraft:white_carpet"},
                {"minecraft:light_gray_carpet", "minecraft:light_gray_carpet"},
                {"minecraft:web", "minecraft:web"}};
        return blocks;
    }

    const std::unordered_map<std::string, std::string> &variantBlocks() {
        static const std::unordered_map<std::string, std::string> blocks = {
                {"minecraft:stone:stone", "minecraft:stone"},
                {"minecraft:stone:smooth_andesite", "minecraft:polished_andesite"},
                {"minecraft:stonebrick:stonebrick", "minecraft:stone_bricks"},
                {"minecraft:stonebrick:mossy_stonebrick", "minecraft:mossy_stone_bricks"},
                {"minecraft:stonebrick:cracked_stonebrick", "minecraft:cracked_stone_bricks"},
                {"minecraft:stonebrick:chiseled_stonebrick", "minecraft:chiseled_stone_bricks"},
                {"minecraft:monster_egg:stone_brick", "minecraft:infested_stone_bricks"},
                {"minecraft:monster_egg:mossy_brick", "minecraft:infested_mossy_stone_bricks"},
                {"minecraft:monster_egg:chiseled_brick", "minecraft:infested_chiseled_stone_bricks"}};
        return blocks;
    }

    std::string prefixed(const std::string &species, const char *suffix) {
        return "minecraft:" + species + suffix;
    }

    // the oak of these two blocks has kept the name it had before the species got their own blocks
    std::string trapdoorName(const std::string &species) {
        return species == "oak" ? "minecraft:trapdoor" : prefixed(species, "_trapdoor");
    }

    std::string doorName(const std::string &species) {
        return species == "oak" ? "minecraft:wooden_door" : prefixed(species, "_door");
    }

    bool translateWood(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns,
                       const StructureBlockTable::WoodSpecies &wood, BlockState &out) {
        const std::string &name = entry.mName;
        const std::string variant = property(entry, "variant");
        const std::string &species = (name.rfind("minecraft:dark_oak", 0) == 0 || variant == "dark_oak") ? wood.mDarkOak
                                                                                                         : wood.mOak;
        Tag states = Tag::ofCompound();
        if (name == "minecraft:planks" && (variant == "oak" || variant == "dark_oak")) {
            out = BlockState(prefixed(species, "_planks"));
            return true;
        }

        if (name == "minecraft:fence" || name == "minecraft:dark_oak_fence") {
            out = BlockState(prefixed(species, "_fence"), states);
            return true;
        }

        if ((name == "minecraft:wooden_slab" || name == "minecraft:double_wooden_slab")
            && (variant == "oak" || variant == "dark_oak")) {
            states.putString("minecraft:vertical_half", property(entry, "half").empty() ? "bottom" : property(entry, "half"));
            out = BlockState(prefixed(species, name == "minecraft:wooden_slab" ? "_slab" : "_double_slab"), states);
            return true;
        }

        if (name == "minecraft:oak_stairs" || name == "minecraft:dark_oak_stairs") {
            states.putString("minecraft:corner", "none");
            states.putByte("upside_down_bit", property(entry, "half") == "top" ? 1 : 0);
            states.putInt("weirdo_direction", weirdoDirection(rotatedFacing(entry, quarterTurns)));
            out = BlockState(prefixed(species, "_stairs"), states);
            return true;
        }

        if (name == "minecraft:stripped_oak_log") {
            std::string axis = property(entry, "axis");
            if ((quarterTurns & 1) != 0 && axis != "y")
                axis = axis == "x" ? "z" : "x";
            states.putString("pillar_axis", axis);
            out = BlockState("minecraft:stripped_" + species + "_log", states);
            return true;
        }

        if (name == "minecraft:trapdoor") {
            states.putInt("direction", weirdoDirection(rotatedFacing(entry, quarterTurns)));
            states.putByte("open_bit", property(entry, "open") == "true" ? 1 : 0);
            states.putByte("upside_down_bit", property(entry, "half") == "top" ? 1 : 0);
            out = BlockState(trapdoorName(species), states);
            return true;
        }

        if (name == "minecraft:wooden_door") {
            // measured: only the lower half keeps a direction, a quarter turn from the side of the file, and the two
            // halves read the hinge of the file opposite ways
            const bool upper = property(entry, "half") == "upper";
            const bool rightHinge = property(entry, "hinge") == "right";
            states.putByte("door_hinge_bit", rightHinge == upper ? 1 : 0);
            states.putString("minecraft:cardinal_direction", upper ? "south" : rotatedFacing(entry, quarterTurns + 1));
            states.putByte("open_bit", property(entry, "open") == "true" ? 1 : 0);
            states.putByte("upper_block_bit", upper ? 1 : 0);
            out = BlockState(doorName(species), states);
            return true;
        }

        return false;
    }
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns,
                                    BlockState &out) {
    return translate(entry, quarterTurns, out, WoodSpecies());
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out,
                                    const WoodSpecies &wood) {
    const std::string &name = entry.mName;

    if (translateWood(entry, quarterTurns, wood, out))
        return true;

    const auto plain = plainBlocks().find(name);
    if (plain != plainBlocks().end()) {
        out = BlockState(plain->second);
        return true;
    }

    const auto variant = variantBlocks().find(name + ":" + property(entry, "variant"));
    if (variant != variantBlocks().end()) {
        out = BlockState(variant->second);
        return true;
    }

    Tag states = Tag::ofCompound();
    if (name == "minecraft:bone_block") {
        const std::string axis = property(entry, "axis");
        const bool swapped = (quarterTurns & 1) != 0 && axis != "y";
        states.putInt("deprecated", 0);
        states.putString("pillar_axis", swapped ? (axis == "x" ? "z" : "x") : axis);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:furnace" || name == "minecraft:chest") {
        states.putString("minecraft:cardinal_direction", rotatedFacing(entry, quarterTurns));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:red_bed") {
        states.putInt("direction", (cardinalIndex(property(entry, "facing")) + quarterTurns) & 3);
        states.putByte("head_piece_bit", property(entry, "part") == "head" ? 1 : 0);
        states.putByte("occupied_bit", 0);
        out = BlockState("minecraft:bed", states);
        return true;
    }

    if (name == "minecraft:ladder" || name == "minecraft:wall_sign") {
        states.putInt("facing_direction", facingDirection(rotatedFacing(entry, quarterTurns)));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:torch") {
        // the file names the side the flame leans to, the state the side of the wall behind it
        states.putString("torch_facing_direction", rotatedFacing(entry, quarterTurns + 2));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:redstone_torch") {
        states.putString("torch_facing_direction", "top");
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:oak_trapdoor") {
        states.putInt("direction", weirdoDirection(rotatedFacing(entry, quarterTurns)));
        states.putByte("open_bit", property(entry, "open") == "true" ? 1 : 0);
        states.putByte("upside_down_bit", property(entry, "half") == "top" ? 1 : 0);
        out = BlockState("minecraft:trapdoor", states);
        return true;
    }

    if (name == "minecraft:spruce_stairs") {
        // the file keeps the corner the stairs had in the editor, the game saves them as plain stairs
        states.putString("minecraft:corner", "none");
        states.putByte("upside_down_bit", property(entry, "half") == "top" ? 1 : 0);
        states.putInt("weirdo_direction", weirdoDirection(rotatedFacing(entry, quarterTurns)));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:wooden_slab" && property(entry, "variant") == "spruce") {
        states.putString("minecraft:vertical_half", property(entry, "half"));
        out = BlockState("minecraft:spruce_slab", states);
        return true;
    }

    if (name == "minecraft:carpet" && property(entry, "color") == "red") {
        out = BlockState("minecraft:red_carpet");
        return true;
    }

    if (name == "minecraft:cauldron") {
        // the two-thirds level of the file is four sixths here
        states.putString("cauldron_liquid", "water");
        states.putInt("fill_level", std::stoi(property(entry, "level")) * 2);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:brewing_stand") {
        // measured: the bottle of the file lands in the first slot, whichever slot it names
        states.putByte("brewing_stand_slot_a_bit", property(entry, "has_bottle_1") == "true" ? 1 : 0);
        states.putByte("brewing_stand_slot_b_bit", property(entry, "has_bottle_0") == "true" ? 1 : 0);
        states.putByte("brewing_stand_slot_c_bit", property(entry, "has_bottle_2") == "true" ? 1 : 0);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:flower_pot") {
        states.putByte("update_bit", 0);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:iron_bars") {
        for (const char *side : {"east", "north", "south", "west"})
            states.putByte(std::string("minecraft:connection_") + side, property(entry, side) == "true" ? 1 : 0);
        out = BlockState(name, states);
        return true;
    }

    return false;
}
