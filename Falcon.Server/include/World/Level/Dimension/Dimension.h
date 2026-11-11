#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// custom dimensions take ids from FIRST_CUSTOM_ID on, cast to this type
enum class DimensionType : int32_t { Overworld = 0, Nether = 1, TheEnd = 2 };

class Dimension {
public:
    static constexpr double NETHER_COORDINATE_SCALE = 8.0;

    // sub_14382DA00: ids above 999 name custom dimensions; 3 is Undefined
    static constexpr int32_t FIRST_CUSTOM_ID = 1000;

    // registered custom dimensions: namespaced name by id
    static std::vector<std::pair<int32_t, std::string>> &customDimensions() {
        static std::vector<std::pair<int32_t, std::string>> dimensions;
        return dimensions;
    }

    static bool isCustom(DimensionType dimension) {
        return toId(dimension) >= FIRST_CUSTOM_ID;
    }

    // the id a name already has, the saved one when given, or the next free one
    static DimensionType registerCustom(const std::string &name, int32_t savedId = -1) {
        bool found = false;
        const DimensionType existing = findCustom(name, found);
        if (found)
            return existing;

        int32_t id = FIRST_CUSTOM_ID;
        for (const auto &entry: customDimensions())
            id = entry.first >= id ? entry.first + 1 : id;
        if (savedId >= FIRST_CUSTOM_ID && customName((DimensionType) savedId) == nullptr)
            id = savedId;
        customDimensions().emplace_back(id, name);
        return (DimensionType) id;
    }

    static DimensionType findCustom(const std::string &name, bool &found) {
        for (const auto &entry: customDimensions()) {
            if (entry.second == name) {
                found = true;
                return (DimensionType) entry.first;
            }
        }
        found = false;
        return DimensionType::Overworld;
    }

    static const std::string *customName(DimensionType dimension) {
        for (const auto &entry: customDimensions()) {
            if (entry.first == toId(dimension))
                return &entry.second;
        }
        return nullptr;
    }

    static DimensionType fromId(int32_t id) {
        if (id == 1) {
            return DimensionType::Nether;
        }

        if (id == 2) {
            return DimensionType::TheEnd;
        }

        if (customName((DimensionType) id) != nullptr)
            return (DimensionType) id;

        return DimensionType::Overworld;
    }

    static int32_t toId(DimensionType dimension) {
        return (int32_t) dimension;
    }

    static const char *getName(DimensionType dimension) {
        const std::string *custom = customName(dimension);
        if (custom != nullptr)
            return custom->c_str();
        switch (dimension) {
            case DimensionType::Nether:
                return "nether";
            case DimensionType::TheEnd:
                return "the_end";
            default:
                return "overworld";
        }
    }

    // sub_14C703FD0: a script dimension spans -64 to 320, like the overworld
    static int32_t getMinY(DimensionType dimension) {
        if (dimension == DimensionType::Overworld || isCustom(dimension)) {
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
        if (dimension == DimensionType::Overworld || isCustom(dimension)) {
            return 0;
        }

        return 4;
    }

    static bool hasSkyLight(DimensionType dimension) {
        return dimension == DimensionType::Overworld || isCustom(dimension);
    }

    static bool hasWeather(DimensionType dimension) {
        return dimension == DimensionType::Overworld;
    }
};
