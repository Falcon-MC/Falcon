#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <cstdint>
#include <string>

/**
 * Turns the palette entries of a structure template into the block states of the server. Every state is checked
 * against instances saved by the game, so the table only holds the blocks of the structures already generated.
 */
class StructureBlockTable {
public:
    // the wood species the oak and dark oak of a template are built with, as block name prefixes
    struct WoodSpecies {
        std::string mOak = "oak";
        std::string mDarkOak = "dark_oak";
    };

    // quarter turns are clockwise seen from above; false for a block with no entry, which is then left out
    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out);

    static bool translate(const StructureTemplate::PaletteEntry &entry, int32_t quarterTurns, BlockState &out,
                          const WoodSpecies &wood);
};
