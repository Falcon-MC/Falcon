#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <cstdint>
#include <string>

// every state checked against instances saved by the game: only the blocks of the structures generated so far
class StructureBlockTable {
public:
    // the wood species the oak and dark oak of a template are built with, as block name prefixes
    struct WoodSpecies {
        std::string mOak = "oak";
        std::string mDarkOak = "dark_oak";
    };

    // quarter turns clockwise seen from above; false for a block with no entry: it is left out
    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out);

    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out,
                          const WoodSpecies &wood);

    // mirror as the template settings store it, applied before the turn: 1 swaps north and south, 2 east and west;
    // false as well for an oriented block whose mirrored state is not known
    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                          BlockState &out);

    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                          BlockState &out, const WoodSpecies &wood);

    // the banner Type the template loader gives a banner without one, from its file block name: 1 is the ominous one
    static int32_t bannerType(const std::string &name);

private:
    static bool _translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, int32_t mirror,
                           BlockState &out, const WoodSpecies &wood);
};
