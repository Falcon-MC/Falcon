#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a village house of 5 by 5 blocks at x and z 0 to 4, its floor at y 64
    const TerrainBeard::Box HOUSE = {0, 64, 0, 4, 70, 4};

    // the game beards a village piece over its box grown by 2 on x and z
    void villagePieceBeardsItsGrownBox() {
        TerrainBeard beard;
        beard.addVillagePiece(0, 0, HOUSE, 64);
        require(!beard.empty(), "the house next to the chunk is not kept");

        const float atBox = beard.adjust(2, 66, 2, 1.0f);
        require(atBox < 0.0f, "over the floor the beard does not empty the ground");
        require(beard.adjust(-2, 66, 2, 1.0f) == atBox, "2 blocks west of the house the beard is not the house's");
        require(beard.adjust(6, 66, 2, 1.0f) == atBox, "2 blocks east of the house the beard is not the house's");
        require(beard.adjust(7, 66, 2, 1.0f) > atBox, "3 blocks east of the house the beard is as strong as on it");

        const float below = beard.adjust(2, 60, 2, -1.0f);
        require(below > 0.0f, "under the floor the beard does not fill the ground");
    }

    // which chunks get it is decided on the box itself, before it grows
    void villagePieceReachesTwelveBlocksFromItsBox() {
        TerrainBeard far;
        far.addVillagePiece(1, 0, {0, 64, 0, 3, 70, 3}, 64);
        require(far.empty(), "a house 13 blocks from the chunk is kept");

        TerrainBeard near;
        near.addVillagePiece(1, 0, HOUSE, 64);
        require(!near.empty(), "a house 12 blocks from the chunk is not kept");
    }
}

int main() {
    try {
        villagePieceBeardsItsGrownBox();
        villagePieceReachesTwelveBlocksFromItsBox();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
