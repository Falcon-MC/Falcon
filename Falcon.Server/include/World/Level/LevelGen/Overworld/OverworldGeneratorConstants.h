#pragma once

#include <cstdint>

class OverworldGeneratorConstants {
public:
    static constexpr int32_t SEA_LEVEL = 63;
    static constexpr int32_t MIN_Y = -64;
    static constexpr int32_t MAX_Y = 319;
    static constexpr int32_t CELL_XZ_SIZE = 4;
    static constexpr int32_t CELL_HEIGHT = 8;
    static constexpr int32_t CELL_X_COUNT = 16 / CELL_XZ_SIZE;
    static constexpr int32_t CELL_Z_COUNT = 16 / CELL_XZ_SIZE;
    static constexpr int32_t CORNER_FLOOD_SEED_MAX_Y = 192;
};
