#pragma once

#include <cstdint>
#include <string>

class BiomeRegistry;
class BlockState;

// what MapColorComponent::getTintMethod picks the multiplier from (sub_145D3D260)
enum class MapTintMethod : uint8_t {
    None = 0,
    DefaultFoliage = 1,
    BirchFoliage = 2,
    EvergreenFoliage = 3,
    DryFoliage = 4,
    Grass = 5,
    Water = 6,
};

struct MapBiomeColors {
    float mTemperature = 0.5f;
    float mDownfall = 0.5f;
    uint32_t mWaterColor = 0xff44aff5u;
};

// block map colours from the 26.60 registration (sub_14CE9AD30): mMapColor, MapColorComponent, LogBlock pairs
namespace MapColors {
    struct Entry {
        const char *mName;
        uint8_t mColor[4];
        // LogBlock (sub_149C45090): the second colour shows when the log lies on its side
        uint8_t mSideColor[4];
        MapTintMethod mTint;
        bool mLog;
    };

    const Entry *find(const std::string &name);

    // a block without a colour gives four zeros, which the renderer skips like the game's empty colour
    void getColor(const BlockState &state, const MapBiomeColors &biome, float out[4]);

    // sub_145D3CB30: the foliage colour map corner blend
    uint32_t foliageColor(float temperature, float downfall);

    // sub_145D3CDA0 without the swamp noise
    uint32_t grassColor(float temperature, float downfall);

    uint32_t tintColor(MapTintMethod tint, const MapBiomeColors &biome);

    // the defaults for an id the registry lacks
    MapBiomeColors biomeColors(const BiomeRegistry &registry, uint32_t biomeId);

    bool isWater(const std::string &name);

    bool isLiquid(const std::string &name);
}
