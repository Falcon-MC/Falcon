#include "World/Level/LevelGen/Structure/StructureBlockTable.h"

#include "World/Level/Block/BlockPaletteRegistry.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

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

    bool endsWith(const std::string &value, const char *suffix) {
        const size_t length = std::char_traits<char>::length(suffix);
        return value.size() > length && value.compare(value.size() - length, length, suffix) == 0;
    }

    std::string property(const StructureTemplate::PaletteEntry &entry, const std::string &name) {
        const auto found = entry.mProperties.find(name);
        return found == entry.mProperties.end() ? std::string() : found->second;
    }

    std::string rotatedFacing(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns) {
        return CARDINALS[(size_t) ((cardinalIndex(property(entry, "facing")) + quarterTurns) & 3)];
    }

    int32_t facingDirection(const std::string &facing) {
        static const std::unordered_map<std::string, int32_t> values = {
                {"north", 2}, {"south", 3}, {"west", 4}, {"east", 5}};
        return values.at(facing);
    }

    int32_t weirdoDirection(const std::string &facing) {
        static const std::unordered_map<std::string, int32_t> values = {
                {"east", 0}, {"west", 1}, {"south", 2}, {"north", 3}};
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
            states.putString("minecraft:vertical_half",
                             property(entry, "half").empty() ? "bottom" : property(entry, "half"));
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

    // a facing_direction value: down 0, up 1, north 2, south 3, west 4, east 5; horizontal faces turn
    int32_t sixFacing(const std::string &facing, int32_t quarterTurns) {
        if (facing == "down")
            return 0;
        if (facing == "up")
            return 1;
        return facingDirection(CARDINALS[(size_t) ((cardinalIndex(facing) + quarterTurns) & 3)]);
    }

    // renames whose bedrock block carries the same states, so the generic pass can finish them
    const std::unordered_map<std::string, std::string> &generalNames() {
        static const std::unordered_map<std::string, std::string> names = {
                {"minecraft:bricks", "minecraft:brick_block"},
                {"minecraft:terracotta", "minecraft:hardened_clay"},
                {"minecraft:grass", "minecraft:short_grass"},
                {"minecraft:melon", "minecraft:melon_block"},
                {"minecraft:magma_block", "minecraft:magma"},
                {"minecraft:cobblestone_stairs", "minecraft:stone_stairs"},
                {"minecraft:oak_fence_gate", "minecraft:fence_gate"},
                {"minecraft:oak_pressure_plate", "minecraft:wooden_pressure_plate"},
                {"minecraft:attached_melon_stem", "minecraft:melon_stem"}};
        return names;
    }

    // maps one template property onto the state the resolved block carries; false when the property has no
    // known mapping, so the caller leaves the block out rather than placing a wrong one
    bool applyProperty(Tag &states, const std::string &key, const std::string &value, int32_t quarterTurns) {
        // states the game recomputes from neighbours, reads from block layer 1, or does not carry
        if (key == "waterlogged" || key == "distance" || key == "snowy" || key == "persistent" || key == "stage"
            || key == "signal_fire" || key == "occupied" || key == "has_book" || key == "east" || key == "north"
            || key == "south" || key == "west" || key == "up" || key == "variant" || key == "powered")
            return true;

        if (key == "axis") {
            if (!states.contains("pillar_axis"))
                return false;
            std::string axis = value;
            if ((quarterTurns & 1) != 0 && axis != "y")
                axis = axis == "x" ? "z" : "x";
            states.putString("pillar_axis", axis);
            return true;
        }

        if (key == "half") {
            if (states.contains("minecraft:vertical_half")) {
                states.putString("minecraft:vertical_half", value == "top" ? "top" : "bottom");
                return true;
            }
            if (states.contains("upside_down_bit")) {
                states.putByte("upside_down_bit", value == "top" ? 1 : 0);
                return true;
            }
            if (states.contains("upper_block_bit")) {
                states.putByte("upper_block_bit", value == "upper" ? 1 : 0);
                return true;
            }
            return false;
        }

        if (key == "type") {
            if (value == "double" || !states.contains("minecraft:vertical_half"))
                return false;
            states.putString("minecraft:vertical_half", value == "top" ? "top" : "bottom");
            return true;
        }

        if (key == "shape") {
            if (!states.contains("weirdo_direction"))
                return false;
            if (value != "straight" && value != "inner_left" && value != "inner_right" && value != "outer_left"
                && value != "outer_right")
                return false;
            states.putString("minecraft:corner", value == "straight" ? "none" : value);
            return true;
        }

        if (key == "open" || key == "in_wall") {
            const char *bit = key == "open" ? "open_bit" : "in_wall_bit";
            if (!states.contains(bit))
                return false;
            states.putByte(bit, value == "true" ? 1 : 0);
            return true;
        }

        if (key == "lit") {
            if (states.contains("lit")) {
                states.putByte("lit", value == "true" ? 1 : 0);
                return true;
            }
            if (states.contains("extinguished")) {
                states.putByte("extinguished", value == "true" ? 0 : 1);
                return true;
            }
            return false;
        }

        if (key == "hanging") {
            if (!states.contains("hanging"))
                return false;
            states.putByte("hanging", value == "true" ? 1 : 0);
            return true;
        }

        if (key == "moisture") {
            if (!states.contains("moisturized_amount"))
                return false;
            states.putInt("moisturized_amount", std::atoi(value.c_str()));
            return true;
        }

        if (key == "level") {
            for (const char *level: {"composter_fill_level", "liquid_depth"}) {
                if (states.contains(level)) {
                    states.putInt(level, std::atoi(value.c_str()));
                    return true;
                }
            }
            return false;
        }

        if (key == "age" || key == "growth") {
            if (states.contains("growth")) {
                states.putInt("growth", std::atoi(value.c_str()));
                return true;
            }
            if (states.contains("age")) {
                states.putInt("age", std::atoi(value.c_str()));
                return true;
            }
            return false;
        }

        if (key == "facing") {
            const std::string rotated = CARDINALS[(size_t) ((cardinalIndex(value) + quarterTurns) & 3)];
            if (states.contains("minecraft:cardinal_direction")) {
                states.putString("minecraft:cardinal_direction", rotated);
                return true;
            }
            if (states.contains("weirdo_direction")) {
                states.putInt("weirdo_direction", weirdoDirection(rotated));
                return true;
            }
            if (states.contains("facing_direction")) {
                states.putInt("facing_direction", sixFacing(value, quarterTurns));
                return true;
            }
            // an int direction with its own basis (loom, bell, grindstone) needs its own case
            return false;
        }

        return false;
    }

    // falls back to the block's default states and maps each template property onto them; the caller only
    // places the block when every property is understood
    bool translateGeneric(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out) {
        std::string name = entry.mName;
        const auto renamed = generalNames().find(name);
        if (renamed != generalNames().end())
            name = renamed->second;

        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        if (defaults == nullptr)
            return false;

        // doors, beds and neighbour-connected blocks carry a direction basis or a shape the game only works out
        // on placement, so the generic path leaves them to their own case
        if (defaults->contains("door_hinge_bit") || defaults->contains("head_piece_bit")
            || defaults->contains("minecraft:connection_east") || defaults->contains("wall_connection_type_east"))
            return false;

        Tag states = *defaults;
        for (const auto &kv: entry.mProperties)
            if (!applyProperty(states, kv.first, kv.second, quarterTurns))
                return false;

        out = BlockState(name, states);
        return true;
    }

    // oriented states sub_146993CE0 reaches through paths not ported here: such a block is left out when mirrored
    const std::array<const char *, 15> UNMIRRORED_STATES = {"facing_direction",
                                                            "direction",
                                                            "torch_facing_direction",
                                                            "pillar_axis",
                                                            "rail_direction",
                                                            "lever_direction",
                                                            "door_hinge_bit",
                                                            "ground_sign_direction",
                                                            "coral_direction",
                                                            "minecraft:facing_direction",
                                                            "minecraft:block_face",
                                                            "orientation",
                                                            "portal_axis",
                                                            "multi_face_direction_bits",
                                                            "brewing_stand_slot_a_bit"};

    void swapProperties(std::map<std::string, std::string> &properties, const char *first, const char *second) {
        const auto a = properties.find(first);
        const auto b = properties.find(second);
        const bool hasA = a != properties.end();
        const bool hasB = b != properties.end();
        const std::string valueA = hasA ? a->second : std::string();
        const std::string valueB = hasB ? b->second : std::string();
        properties.erase(first);
        properties.erase(second);
        if (hasA)
            properties[second] = valueA;
        if (hasB)
            properties[first] = valueB;
    }
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns,
                                    BlockState &out) {
    return _translate(entry, quarterTurns, 0, out, WoodSpecies());
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out,
                                    const WoodSpecies &wood) {
    return _translate(entry, quarterTurns, 0, out, wood);
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                                    BlockState &out) {
    return translate(entry, quarterTurns, mirror, out, WoodSpecies());
}

bool StructureBlockTable::translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                                    BlockState &out, const WoodSpecies &wood) {
    if (mirror == 0)
        return _translate(entry, quarterTurns, 0, out, wood);
    // sub_146993CE0 mirrors before it turns: a cardinal_direction outside doors, a weirdo_direction, the kept vine
    // face and the connection sides (sub_145E93340); the coordinates only flip for 1 and 2 (sub_14775E430)
    if (mirror != 1 && mirror != 2)
        return false;

    const char *first = mirror == 1 ? "north" : "east";
    const char *second = mirror == 1 ? "south" : "west";
    StructureTemplate::PaletteEntry mirrored = entry;
    const auto facing = mirrored.mProperties.find("facing");
    if (facing != mirrored.mProperties.end()) {
        if (facing->second == first)
            facing->second = second;
        else if (facing->second == second)
            facing->second = first;
    }
    // the vine picks its face from the file first
    if (entry.mName != "minecraft:vine")
        swapProperties(mirrored.mProperties, first, second);

    if (!_translate(mirrored, quarterTurns, mirror, out, wood))
        return false;
    for (const char *state: UNMIRRORED_STATES) {
        if (out.mStates.contains(state))
            return false;
    }
    return true;
}

bool StructureBlockTable::_translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                                     BlockState &out, const WoodSpecies &wood) {
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

    if (name == "minecraft:furnace" || name == "minecraft:smoker" || name == "minecraft:blast_furnace"
        || name == "minecraft:chest") {
        states.putString("minecraft:cardinal_direction", rotatedFacing(entry, quarterTurns));
        out = BlockState(name, states);
        return true;
    }

    if (name.size() > 4 && name.compare(name.size() - 4, 4, "_bed") == 0) {
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

    // the dye goes to the block actor (Base)
    if (name.size() > 12 && name.compare(name.size() - 12, 12, "_wall_banner") == 0) {
        states.putInt("facing_direction", facingDirection(rotatedFacing(entry, quarterTurns)));
        out = BlockState("minecraft:wall_banner", states);
        return true;
    }

    if (name == "minecraft:torch") {
        // standing by name (sub_1469994B0); a file facing is the flame side, the state the wall side
        states.putString("torch_facing_direction", entry.mProperties.count("facing") == 0
                                                           ? std::string("top")
                                                           : rotatedFacing(entry, quarterTurns + 2));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:redstone_torch") {
        states.putString("torch_facing_direction", "top");
        out = BlockState(name, states);
        return true;
    }

    if (endsWith(name, "_trapdoor")) {
        states.putInt("direction", weirdoDirection(rotatedFacing(entry, quarterTurns)));
        states.putByte("open_bit", property(entry, "open") == "true" ? 1 : 0);
        states.putByte("upside_down_bit", property(entry, "half") == "top" ? 1 : 0);
        out = BlockState(name == "minecraft:oak_trapdoor" ? std::string("minecraft:trapdoor") : name, states);
        return true;
    }

    if (endsWith(name, "_button")) {
        const std::string face = property(entry, "face");
        states.putByte("button_pressed_bit", property(entry, "powered") == "true" ? 1 : 0);
        states.putInt("facing_direction", face == "floor"     ? 1
                                          : face == "ceiling" ? 0
                                                              : facingDirection(rotatedFacing(entry, quarterTurns)));
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:snow") {
        states.putByte("covered_bit", 0);
        states.putInt("height", std::max(std::atoi(property(entry, "layers").c_str()), 1) - 1);
        out = BlockState("minecraft:snow_layer", states);
        return true;
    }

    if (name == "minecraft:grindstone") {
        static const std::unordered_map<std::string, std::string> FACES = {
                {"floor", "standing"}, {"wall", "side"}, {"ceiling", "hanging"}};
        const auto face = FACES.find(property(entry, "face"));
        if (face == FACES.end())
            return false;
        states.putString("attachment", face->second);
        states.putInt("direction", (cardinalIndex(property(entry, "facing")) + quarterTurns) & 3);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:stonecutter") {
        states.putString("minecraft:cardinal_direction", rotatedFacing(entry, quarterTurns));
        out = BlockState("minecraft:stonecutter_block", states);
        return true;
    }

    if (name == "minecraft:sea_pickle") {
        // a pickle out of water is dead
        states.putInt("cluster_count", std::max(std::atoi(property(entry, "pickles").c_str()), 1) - 1);
        states.putByte("dead_bit", property(entry, "waterlogged") == "true" ? 0 : 1);
        out = BlockState(name, states);
        return true;
    }

    // the plant goes to the block actor (PlantBlock)
    if (name.rfind("minecraft:potted_", 0) == 0) {
        states.putByte("update_bit", 0);
        out = BlockState("minecraft:flower_pot", states);
        return true;
    }

    // shaped from neighbours once placed
    if (endsWith(name, "_wall")) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        if (defaults == nullptr || !defaults->contains("wall_connection_type_east"))
            return false;
        out = BlockState(name, *defaults);
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

    if (name == "minecraft:vine") {
        // sub_1469957B0 sets the bits of each true face in key order: the last one wins, up standing for all four
        static const std::array<std::pair<const char *, int32_t>, 5> FACES = {
                {{"east", 8}, {"north", 4}, {"south", 1}, {"up", 15}, {"west", 2}}};
        int32_t bits = 0;
        for (const std::pair<const char *, int32_t> &face: FACES) {
            if (property(entry, face.first) == "true")
                bits = face.second;
        }
        // the template transform (sub_146993CE0) keeps one face, south then north, east, west, north when none,
        // and turns it clockwise: north, east, south, west
        static const std::array<int32_t, 4> CLOCKWISE_BITS = {4, 8, 1, 2};
        int32_t face = (bits & 1) != 0 ? 2 : (bits & 4) != 0 ? 0 : (bits & 8) != 0 ? 1 : (bits & 2) != 0 ? 3 : 0;
        if ((mirror == 1 && (face & 1) == 0) || (mirror == 2 && (face & 1) != 0))
            face ^= 2;
        states.putInt("vine_direction_bits", CLOCKWISE_BITS[(size_t) ((face + quarterTurns) & 3)]);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:iron_bars") {
        // the sides turn with the template (sub_145E93340); written in palette key order
        static const std::array<const char *, 4> CLOCKWISE = {"north", "east", "south", "west"};
        for (const size_t side: {(size_t) 1, (size_t) 0, (size_t) 2, (size_t) 3}) {
            const char *from = CLOCKWISE[(side + 4 - (size_t) (quarterTurns & 3)) & 3];
            states.putByte(std::string("minecraft:connection_") + CLOCKWISE[side],
                           property(entry, from) == "true" ? 1 : 0);
        }
        out = BlockState(name, states);
        return true;
    }

    // measured on BDS 1.26.61 villages
    const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
    if (defaults != nullptr && defaults->contains("minecraft:connection_east")) {
        // shaped from neighbours once placed
        out = BlockState(name, *defaults);
        return true;
    }

    if (name.size() > 5 && name.compare(name.size() - 5, 5, "_door") == 0) {
        // lower half: direction; upper half: hinge
        const bool upper = property(entry, "half") == "upper";
        const std::string doorName = name == "minecraft:oak_door" ? "minecraft:wooden_door" : name;
        const Tag *doorDefaults = BlockPaletteRegistry::getInstance().getDefaultStates(doorName);
        if (doorDefaults == nullptr)
            return false;
        // defaults first: palette key order
        states = *doorDefaults;
        states.putString(
                "minecraft:cardinal_direction",
                upper ? "south"
                      : CARDINALS[(size_t) ((cardinalIndex(property(entry, "facing")) + quarterTurns + 1) & 3)]);
        states.putByte("door_hinge_bit", upper && property(entry, "hinge") == "right" ? 1 : 0);
        states.putByte("open_bit", property(entry, "open") == "true" ? 1 : 0);
        states.putByte("upper_block_bit", upper ? 1 : 0);
        out = BlockState(doorName, states);
        return true;
    }

    if (name == "minecraft:wall_torch") {
        states.putString("torch_facing_direction", rotatedFacing(entry, quarterTurns + 2));
        out = BlockState("minecraft:torch", states);
        return true;
    }

    if (name == "minecraft:loom") {
        states.putInt("direction", (cardinalIndex(property(entry, "facing")) + quarterTurns + 2) & 3);
        out = BlockState(name, states);
        return true;
    }

    if (name == "minecraft:bell") {
        static const std::unordered_map<std::string, std::string> ATTACHMENTS = {
                {"floor", "standing"}, {"ceiling", "hanging"}, {"single_wall", "side"}, {"double_wall", "multiple"}};
        const auto attachment = ATTACHMENTS.find(property(entry, "attachment"));
        if (attachment == ATTACHMENTS.end())
            return false;
        states.putString("attachment", attachment->second);
        states.putInt("direction", (cardinalIndex(property(entry, "facing")) + quarterTurns) & 3);
        states.putByte("toggle_bit", 0);
        out = BlockState(name, states);
        return true;
    }

    if (property(entry, "type") == "double" && name.size() > 5 && name.compare(name.size() - 5, 5, "_slab") == 0) {
        const std::string doubled = name.substr(0, name.size() - 5) + "_double_slab";
        if (BlockPaletteRegistry::getInstance().getDefaultStates(doubled) == nullptr)
            return false;
        states.putString("minecraft:vertical_half", "bottom");
        out = BlockState(doubled, states);
        return true;
    }

    return translateGeneric(entry, quarterTurns, out);
}

int32_t StructureBlockTable::bannerType(const std::string &name) {
    // sub_14699A380
    return name.find("illager_captain") != std::string::npos ? 1 : 0;
}
