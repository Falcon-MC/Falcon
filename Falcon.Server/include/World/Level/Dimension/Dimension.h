#pragma once

#include <cstdint>

enum class DimensionType : int32_t {
    Overworld = 0,
    Nether = 1,
    TheEnd = 2
};

class Dimension {
public:
    static constexpr int32_t DIMENSION_COUNT = 3;

    static constexpr double NETHER_COORDINATE_SCALE = 8.0;

    static DimensionType fromId(int32_t id) {
        if (id == 1) {
            return DimensionType::Nether;
        }

        if (id == 2) {
            return DimensionType::TheEnd;
        }

        return DimensionType::Overworld;
    }

    static int32_t toId(DimensionType dimension) {
        return (int32_t) dimension;
    }

    static const char *getName(DimensionType dimension) {
        switch (dimension) {
            case DimensionType::Nether:
                return "nether";
            case DimensionType::TheEnd:
                return "the_end";
            default:
                return "overworld";
        }
    }

    static int32_t getMinY(DimensionType dimension) {
        if (dimension == DimensionType::Overworld) {
            return -64;
        }

        return 0;
    }

    static int32_t getMaxY(DimensionType dimension) {
        switch (dimension) {
            case DimensionType::Nether:
                return 127;
            case DimensionType::TheEnd:
                return 255;
            default:
                return 319;
        }
    }

    static int32_t getSubChunkCount(DimensionType dimension) {
        switch (dimension) {
            case DimensionType::Nether:
                return 8;
            case DimensionType::TheEnd:
                return 16;
            default:
                return 24;
        }
    }

    static int32_t getLowestSubChunkIndex(DimensionType dimension) {
        if (dimension == DimensionType::Overworld) {
            return 0;
        }

        return 4;
    }

    static bool hasSkyLight(DimensionType dimension) {
        return dimension == DimensionType::Overworld;
    }

    static bool hasWeather(DimensionType dimension) {
        return dimension == DimensionType::Overworld;
    }
};
