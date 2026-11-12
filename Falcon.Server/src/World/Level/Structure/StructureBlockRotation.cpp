#include "World/Level/Structure/StructureBlockRotation.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
    // the game's 25 orientations: 0-3 north, east, south, west, then axes, diagonals and curves, 24 none
    const int32_t NO_ORIENTATION = 24;

    // sub_145E929F0
    const int32_t ROTATIONS[3][25] = {
            {1, 2, 3, 0, 5, 4, 8, 6, 9, 7, 11, 10, 14, 12, 15, 13, 19, 18, 16, 17, 21, 22, 23, 20, 24},
            {2, 3, 0, 1, 4, 5, 9, 8, 7, 6, 10, 11, 15, 14, 13, 12, 17, 16, 19, 18, 22, 23, 20, 21, 24},
            {3, 0, 1, 2, 5, 4, 7, 9, 6, 8, 11, 10, 13, 15, 12, 14, 18, 19, 17, 16, 23, 20, 21, 22, 24}};

    // sub_145E97A10; the third is the half turn
    const int32_t MIRRORS[3][25] = {
            {2, 1, 0, 3, 4, 5, 6, 8, 7, 9, 10, 11, 12, 14, 13, 15, 16, 17, 19, 18, 23, 22, 21, 20, 24},
            {0, 3, 2, 1, 4, 5, 9, 7, 8, 6, 10, 11, 15, 13, 14, 12, 17, 16, 18, 19, 21, 20, 23, 22, 24},
            {2, 3, 0, 1, 4, 5, 9, 8, 7, 6, 10, 11, 15, 14, 13, 12, 17, 16, 19, 18, 22, 23, 20, 21, 24}};

    // a door's cardinal direction mirrors through qword_14F282A30, qword_14F282A90 and qword_14F282AF0
    const int32_t DOOR_MIRRORS[3][24] = {
            {0, 3, 2, 1, 4, 5, 9, 7, 8, 6, 10, 11, 15, 13, 14, 12, 17, 16, 18, 19, 21, 20, 23, 22},
            {2, 1, 0, 3, 4, 5, 6, 8, 7, 9, 10, 11, 12, 14, 13, 15, 16, 17, 19, 18, 23, 22, 21, 20},
            {2, 3, 0, 1, 4, 5, 9, 8, 7, 6, 10, 11, 15, 14, 13, 12, 17, 16, 19, 18, 22, 23, 20, 21}};

    using ValueTable = std::vector<std::pair<int32_t, int32_t>>;

    // value of the state, orientation; the first match wins both ways
    const ValueTable CARDINAL = {{2, 0}, {3, 1}, {0, 2}, {1, 3}}; // xmmword_14F280FF8
    const ValueTable FACING = {{2, 0}, {5, 1}, {3, 2}, {4, 3}, {1, 24}, {0, 24}}; // byte_14F281018
    const ValueTable PORTAL_AXIS = {{2, 0}, {1, 1}, {2, 2}, {1, 3}}; // xmmword_14F280FD8
    const ValueTable RAIL = {{0, 4},  {1, 5},  {4, 18}, {2, 16}, {5, 19},
                             {3, 17}, {9, 23}, {6, 20}, {7, 21}, {8, 22}}; // xmmword_14F281048
    const ValueTable TORCH = {{3, 0}, {2, 1}, {4, 2}, {1, 3}, {5, 24}, {0, 24}}; // xmmword_14F281098
    const ValueTable WEIRDO = {{3, 0}, {0, 1}, {2, 2}, {1, 3}}; // xmmword_14F2810C8
    const ValueTable CORAL = {{2, 0}, {1, 1}, {3, 2}, {0, 3}}; // xmmword_14F2810E8
    const ValueTable LEVER = {{4, 0}, {1, 1}, {3, 2}, {2, 3}, {0, 5}, {7, 4}, {6, 11}, {5, 10}}; // xmmword_14F281108
    const ValueTable PILLAR = {{2, 0}, {1, 1}, {2, 2}, {1, 3}, {0, 24}}; // xmmword_14F281148
    const ValueTable ORIENTATION = {{0, 6},  {1, 7},  {2, 8}, {3, 9}, {4, 12}, {5, 13},
                                    {6, 14}, {7, 15}, {8, 3}, {9, 1}, {10, 0}, {11, 2}}; // xmmword_14F281170

    // the value names of the string states, by value; their tables above fix the order
    const std::vector<std::string> CARDINAL_NAMES = {"south", "west", "north", "east"};
    const std::vector<std::string> FACING_NAMES = {"down", "up", "north", "south", "west", "east"};
    // the name tables at 0x14F31E048, 0x14F3B3F48 and 0x14F3E4FA8
    const std::vector<std::string> CORNER_NAMES = {"none", "inner_left", "inner_right", "outer_left", "outer_right"};
    const std::vector<std::string> LEVER_NAMES = {
            "down_east_west", "east", "west", "south", "north", "up_north_south", "up_east_west", "down_north_south"};
    const std::vector<std::string> ORIENTATION_NAMES = {"down_east", "down_north", "down_south", "down_west",
                                                        "up_east",   "up_north",   "up_south",   "up_west",
                                                        "west_up",   "east_up",    "north_up",   "south_up"};
    const std::vector<std::string> TORCH_NAMES = {"unknown", "west", "east", "north", "south", "top"};
    const std::vector<std::string> PORTAL_AXIS_NAMES = {"unknown", "x", "z"};
    const std::vector<std::string> PILLAR_NAMES = {"y", "x", "z"};

    // the four sides in orientation order: north, east, south, west
    const char *const WALL_SIDES[4] = {"wall_connection_type_north", "wall_connection_type_east",
                                       "wall_connection_type_south", "wall_connection_type_west"};
    const char *const CONNECTION_SIDES[4] = {"minecraft:connection_north", "minecraft:connection_east",
                                             "minecraft:connection_south", "minecraft:connection_west"};

    // a door (block property 0x200) and a trapdoor (0x2000) read their directions their own way
    bool isDoor(const std::string &name) {
        return name.size() >= 5 && name.compare(name.size() - 5, 5, "_door") == 0;
    }

    bool isTrapdoor(const std::string &name) {
        return name.size() >= 8 && name.compare(name.size() - 8, 8, "trapdoor") == 0;
    }

    struct States {
        Tag &mTag;

        bool has(const char *key) const {
            return mTag.contains(key);
        }

        // the value index of a state, string states through their names
        int32_t read(const char *key, const std::vector<std::string> *names = nullptr) const {
            const Tag *value = mTag.get(key);
            if (value == nullptr)
                return 0;
            switch (value->getType()) {
                case Tag::Type::Byte:
                    return value->asByte();
                case Tag::Type::Int:
                    return value->asInt();
                case Tag::Type::String:
                    if (names != nullptr) {
                        for (size_t i = 0; i < names->size(); ++i) {
                            if ((*names)[i] == value->asString())
                                return (int32_t) i;
                        }
                    }
                    return 0;
                default:
                    return 0;
            }
        }

        void write(const char *key, int32_t index, const std::vector<std::string> *names = nullptr) {
            const Tag *value = mTag.get(key);
            if (value == nullptr)
                return;
            switch (value->getType()) {
                case Tag::Type::Byte:
                    mTag.putByte(key, (int8_t) index);
                    break;
                case Tag::Type::Int:
                    mTag.putInt(key, index);
                    break;
                case Tag::Type::String:
                    if (names != nullptr && index >= 0 && (size_t) index < names->size())
                        mTag.putString(key, (*names)[(size_t) index]);
                    break;
                default:
                    break;
            }
        }
    };

    int32_t orientationOf(const ValueTable &table, int32_t value) {
        for (const auto &entry: table) {
            if (entry.first == value)
                return entry.second;
        }
        return 0;
    }

    int32_t valueOf(const ValueTable &table, int32_t orientation) {
        for (const auto &entry: table) {
            if (entry.second == orientation)
                return entry.first;
        }
        return 0;
    }

    int32_t turn(int32_t orientation, int rotation, int mirror) {
        if (orientation < 0 || orientation > NO_ORIENTATION)
            return 0;
        if (mirror >= 1 && mirror <= 3)
            orientation = MIRRORS[mirror - 1][orientation];
        if (rotation >= 1 && rotation <= 3)
            orientation = ROTATIONS[rotation - 1][orientation];
        return orientation;
    }

    // sub_145E92860, sub_145ECA380 and the per state copies: a value without an orientation stays
    int32_t turnValue(const ValueTable &table, int32_t value, int rotation, int mirror) {
        const int32_t orientation = orientationOf(table, value);
        if (orientation == NO_ORIENTATION)
            return value;
        return valueOf(table, turn(orientation, rotation, mirror));
    }

    void turnState(States &states, const char *key, const ValueTable &table, int rotation, int mirror,
                   const std::vector<std::string> *names = nullptr) {
        if (!states.has(key))
            return;
        states.write(key, turnValue(table, states.read(key, names), rotation, mirror), names);
    }

    // the stair corner swaps left and right when mirrored across its facing axis
    void turnCorner(States &states, int mirror) {
        static const char *const CORNER = "minecraft:corner";
        if (!states.has(CORNER))
            return;
        int32_t axis;
        if (states.has("minecraft:cardinal_direction"))
            axis = orientationOf(CARDINAL, states.read("minecraft:cardinal_direction", &CARDINAL_NAMES)) & ~2;
        else if (states.has("weirdo_direction"))
            axis = orientationOf(WEIRDO, states.read("weirdo_direction")) & ~2;
        else
            return;
        int32_t corner = states.read(CORNER, &CORNER_NAMES);
        if (((mirror == 2 && axis == 1) || (mirror == 1 && axis == 0)) && corner >= 1 && corner <= 4)
            corner = (int32_t) ((0x3040102u >> (8 * (corner - 1))) & 0xFF);
        states.write(CORNER, corner, &CORNER_NAMES);
    }

    void turnCardinal(States &states, const std::string &name, int rotation, int mirror) {
        static const char *const KEY = "minecraft:cardinal_direction";
        if (!states.has(KEY))
            return;
        const int32_t value = states.read(KEY, &CARDINAL_NAMES);
        if (!isDoor(name)) {
            states.write(KEY, turnValue(CARDINAL, value, rotation, mirror), &CARDINAL_NAMES);
            return;
        }
        int32_t orientation = orientationOf(CARDINAL, value);
        if (orientation == NO_ORIENTATION)
            return;
        if (mirror >= 1 && mirror <= 3)
            orientation = DOOR_MIRRORS[mirror - 1][orientation];
        if (rotation >= 1 && rotation <= 3)
            orientation = ROTATIONS[rotation - 1][orientation];
        states.write(KEY, valueOf(CARDINAL, orientation), &CARDINAL_NAMES);
    }

    // a trapdoor's direction counts east, west, south, north (0x2000103, qword_14F0EED00)
    void turnDirection(States &states, const std::string &name, int rotation, int mirror) {
        static const char *const KEY = "direction";
        if (!states.has(KEY))
            return;
        const int32_t value = states.read(KEY);
        if (!isTrapdoor(name)) {
            states.write(KEY, turnValue(CARDINAL, value, rotation, mirror));
            return;
        }
        const int32_t cardinal = (value & 0xFC) != 0 ? 0 : (int32_t) ((0x2000103u >> (8 * value)) & 0xFF);
        const int32_t turned = turnValue(CARDINAL, cardinal, rotation, mirror);
        static const int32_t TRAPDOOR_DIRECTIONS[3] = {2, 1, 3};
        states.write(KEY, turned >= 0 && turned <= 2 ? TRAPDOOR_DIRECTIONS[turned] : 0);
    }

    void turnGroundSign(States &states, int rotation, int mirror) {
        static const char *const KEY = "ground_sign_direction";
        if (!states.has(KEY))
            return;
        const int32_t count = 16;
        const int32_t half = count / 2;
        int32_t value = states.read(KEY);
        switch (mirror) {
            case 1:
                value = (half - value + (half < value ? count : 0)) % count;
                break;
            case 2:
                value = ((half < value ? count : 0) + count - value) % count;
                break;
            case 3: {
                const int32_t flipped = ((half < value ? count : 0) + half - value) % count;
                value = ((flipped > half ? count : 0) + count - flipped) % count;
                break;
            }
            default:
                break;
        }
        states.write(KEY, rotation >= 0 && rotation <= 3 ? (value + 4 * rotation) % count : 0);
    }

    // south 1, west 2, north 4, east 8; a mirror moves a set side onto its opposite one way only
    void turnVineBits(States &states, int rotation, int mirror) {
        static const char *const KEY = "vine_direction_bits";
        if (!states.has(KEY))
            return;
        int32_t value = states.read(KEY);
        switch (mirror) {
            case 1:
                if ((value & 1) != 0)
                    value = (value & 0xA) | 4;
                else if ((value & 4) != 0)
                    value = (value & 0xA) + 1;
                break;
            case 2:
                if ((value & 2) != 0)
                    value = (value & 5) | 8;
                else if ((value & 8) != 0)
                    value = (value & 5) + 2;
                break;
            case 3:
                value = ((value << 2) | (value >> 2)) & 0xF;
                break;
            default:
                break;
        }
        if (rotation >= 1 && rotation <= 3)
            value = (((value << rotation) & 0xFF) | (value >> (4 - rotation))) & 0xF;
        states.write(KEY, value);
    }

    // down 1, up 2, then the vine sides shifted by 2
    void turnMultiFaceBits(States &states, int rotation, int mirror) {
        static const char *const KEY = "multi_face_direction_bits";
        if (!states.has(KEY))
            return;
        const int32_t value = states.read(KEY);
        int32_t sides = value >> 2;
        switch (mirror) {
            case 1:
                if ((value & 0x10) != 0) {
                    if ((value & 4) == 0)
                        sides = ((value >> 2) & 0xA) + 1;
                } else if ((value & 4) != 0) {
                    sides = ((value >> 2) & 0xA) | 4;
                }
                break;
            case 2:
                if ((value & 0x20) != 0) {
                    if ((value & 8) == 0)
                        sides = ((value >> 2) & 5) | 2;
                } else if ((value & 8) != 0) {
                    sides = ((value >> 2) & 5) | 8;
                }
                break;
            case 3:
                sides = (value & 0xC) | ((value & 0xFF) >> 4);
                break;
            default:
                break;
        }
        const int32_t shifted = sides << rotation;
        states.write(KEY, (value & 3) + 4 * (((shifted & 0xFF) | ((shifted >> 4) & 0xFF)) & 0xF));
    }

    // sub_145E92E50 and sub_145E93340: every side present, the values move with the sides
    void turnSides(States &states, const char *const sides[4], int rotation, int mirror) {
        for (int i = 0; i < 4; ++i) {
            if (!states.has(sides[i]))
                return;
        }
        Tag values[4];
        for (int i = 0; i < 4; ++i)
            values[i] = *states.mTag.get(sides[i]);
        Tag mirrored[4] = {values[0], values[1], values[2], values[3]};
        if (mirror == 1 || mirror == 3)
            std::swap(mirrored[0], mirrored[2]);
        if (mirror == 2 || mirror == 3)
            std::swap(mirrored[1], mirrored[3]);
        const int turns = rotation >= 1 && rotation <= 3 ? rotation : 0;
        for (int i = 0; i < 4; ++i)
            states.mTag.put(sides[i], mirrored[(i - turns + 4) % 4]);
    }
}

BlockState StructureBlockRotation::apply(const BlockState &state, int rotation, int mirror) {
    rotation &= 3;
    if (mirror < 0 || mirror > 3)
        mirror = 0;
    if (rotation == 0 && mirror == 0)
        return state;

    Tag tag = state.mStates;
    States states{tag};
    turnCorner(states, mirror);
    turnState(states, "portal_axis", PORTAL_AXIS, rotation, mirror, &PORTAL_AXIS_NAMES);
    turnCardinal(states, state.mName, rotation, mirror);
    turnState(states, "minecraft:facing_direction", FACING, rotation, mirror, &FACING_NAMES);
    turnState(states, "minecraft:block_face", FACING, rotation, mirror, &FACING_NAMES);
    turnDirection(states, state.mName, rotation, mirror);
    turnState(states, "facing_direction", FACING, rotation, mirror);
    turnState(states, "rail_direction", RAIL, rotation, mirror);
    turnState(states, "torch_facing_direction", TORCH, rotation, mirror, &TORCH_NAMES);
    turnGroundSign(states, rotation, mirror);
    turnState(states, "weirdo_direction", WEIRDO, rotation, mirror);
    turnState(states, "coral_direction", CORAL, rotation, mirror);
    turnState(states, "lever_direction", LEVER, rotation, mirror, &LEVER_NAMES);
    turnState(states, "pillar_axis", PILLAR, rotation, mirror, &PILLAR_NAMES);
    turnVineBits(states, rotation, mirror);
    turnMultiFaceBits(states, rotation, mirror);
    if ((mirror == 1 || mirror == 2) && states.has("door_hinge_bit"))
        states.write("door_hinge_bit", states.read("door_hinge_bit") != 0 ? 0 : 1);
    turnSides(states, WALL_SIDES, rotation, mirror);
    turnSides(states, CONNECTION_SIDES, rotation, mirror);
    turnState(states, "orientation", ORIENTATION, rotation, mirror, &ORIENTATION_NAMES);
    return BlockState(state.mName, tag);
}
