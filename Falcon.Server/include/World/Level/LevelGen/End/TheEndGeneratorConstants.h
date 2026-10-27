#pragma once

#include <cstdint>

namespace TheEndGeneratorConstants {

    static constexpr int32_t MIN_Y = 0;
    static constexpr int32_t MAX_Y = 255;

    static constexpr int32_t BIOME_ID = 9;

    static constexpr int32_t SPAWN_Y = 65;

    static constexpr int32_t DENSITY_XZ_COUNT = 3;
    static constexpr int32_t DENSITY_Y_COUNT = 33;

    static constexpr double COORDINATE_SCALE = 684.412;
    static constexpr double DETAIL_NOISE_SCALE_X = 80.0;
    static constexpr double DETAIL_NOISE_SCALE_Z = 80.0;
    static constexpr double DETAIL_NOISE_Y_SCALE = 4.277575000000001;

    static constexpr int64_t MAIN_ISLAND_CHUNK_RADIUS_SQUARED = 4096LL;

}
