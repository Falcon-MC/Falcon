#include "World/Level/LevelGen/Nether/Biome/VanillaNetherBiomeSource.h"

#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"

#include <limits>

namespace {
    const uint64_t LCG_MULTIPLIER = 0x5851F42D4C957F2DULL;
    const uint64_t LCG_INCREMENT = 0x14057B7EF767814FULL;
    const uint64_t ZOOM_SALT = 0x86BAB6D7626F4BD8ULL;

    const float CLIMATE_FREQUENCY = 0.015625f;
    const float SECOND_NOISE_STRETCH = 1.0181268f;
    const float CORNER_SPACING = 4.0f;
    const int32_t CLIMATE_OCTAVES = 2;

    struct ClimatePoint {
        int32_t mBiomeId;
        float mTemperature;
        float mHumidity;
        float mAltitude;
        float mWeirdness;
        float mOffset;
    };

    const ClimatePoint CLIMATE_POINTS[] = {{NetherBiomeIds::BASALT_DELTAS, -0.5f, 0.0f, 0.0f, 0.0f, 0.175f},
                                           {NetherBiomeIds::CRIMSON_FOREST, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f},
                                           {NetherBiomeIds::HELL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                                           {NetherBiomeIds::SOULSAND_VALLEY, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f},
                                           {NetherBiomeIds::WARPED_FOREST, 0.0f, 0.5f, 0.0f, 0.0f, 0.375f}};

    uint64_t step(uint64_t value) {
        return value * (LCG_MULTIPLIER * value + LCG_INCREMENT);
    }

    float jitterFrom(uint64_t state) {
        const int32_t value = (int32_t) (((int64_t) state >> 24) & 1023);
        return ((float) value * 0.0009765625f + -0.5f) * 3.6f;
    }
}

VanillaNetherBiomeSource::ClimateNoise::ClimateNoise(std::mt19937 random) : ClimateNoise(random, CLIMATE_OCTAVES) {}

VanillaNetherBiomeSource::ClimateNoise::ClimateNoise(std::mt19937 &random, int32_t octaves)
        : mFirst(random, octaves), mSecond(random, octaves) {}

float VanillaNetherBiomeSource::ClimateNoise::getValue(int32_t gridX, int32_t gridZ) const {
    const float x = (float) gridX;
    const float z = (float) gridZ;
    const float first = mFirst.sample(x * CLIMATE_FREQUENCY, 0.0f, z * CLIMATE_FREQUENCY);
    const float second = mSecond.sample((x * SECOND_NOISE_STRETCH) * CLIMATE_FREQUENCY, 0.0f,
                                        (z * SECOND_NOISE_STRETCH) * CLIMATE_FREQUENCY);

    const double spread = (1.0 / (double) (CLIMATE_OCTAVES + 1) + 1.0) * 0.1;
    return (second + first) * (float) ((double) 0.16666667f / spread);
}

VanillaNetherBiomeSource::VanillaNetherBiomeSource(int64_t seed)
        : mNoises{ClimateNoise(std::mt19937((uint32_t) seed)), ClimateNoise(std::mt19937((uint32_t) seed + 1)),
                  ClimateNoise(std::mt19937((uint32_t) seed + 2)), ClimateNoise(std::mt19937((uint32_t) seed + 3))} {
    const uint64_t first = ZOOM_SALT + step((uint64_t) seed);
    const uint64_t second = ZOOM_SALT + step(first);
    mZoomSeed = ZOOM_SALT + step(second);
}

int32_t VanillaNetherBiomeSource::_pickClimateBiome(int32_t gridX, int32_t gridZ) const {
    const float temperature = mNoises[0].getValue(gridX, gridZ);
    const float humidity = mNoises[1].getValue(gridX, gridZ);
    const float altitude = mNoises[2].getValue(gridX, gridZ);
    const float weirdness = mNoises[3].getValue(gridX, gridZ);

    float best = std::numeric_limits<float>::max();
    int32_t biomeId = NetherBiomeIds::HELL;

    for (const ClimatePoint &point: CLIMATE_POINTS) {
        const float deltaTemperature = point.mTemperature - temperature;
        const float deltaHumidity = point.mHumidity - humidity;
        const float deltaAltitude = point.mAltitude - altitude;
        const float deltaWeirdness = point.mWeirdness - weirdness;
        const float distance = point.mOffset * point.mOffset
                               + (deltaWeirdness * deltaWeirdness
                                  + ((deltaHumidity * deltaHumidity + deltaTemperature * deltaTemperature)
                                     + deltaAltitude * deltaAltitude));
        if (best > distance) {
            best = distance;
            biomeId = point.mBiomeId;
        }
    }

    return biomeId;
}

void VanillaNetherBiomeSource::_jitter(int32_t gridX, int32_t gridZ, float &x, float &z) const {
    const uint64_t blockX = (uint64_t) ((int64_t) gridX * 4);
    const uint64_t blockZ = (uint64_t) ((int64_t) gridZ * 4);

    uint64_t state = step(mZoomSeed);
    state = step(state + blockX);
    state = step(state + blockZ);
    state = step(state + blockX) + blockZ;

    x = jitterFrom(state);
    z = jitterFrom(mZoomSeed + step(state));
}

int32_t VanillaNetherBiomeSource::_nearestCorner(const float *cornerX, const float *cornerZ, int32_t localX,
                                                 int32_t localZ) {
    float distances[4];
    for (int32_t i = 0; i < 4; i++) {
        const float x = cornerX[i] - (float) localX;
        const float z = cornerZ[i] - (float) localZ;
        distances[i] = x * x + z * z;
    }

    if (distances[3] > distances[0] && distances[1] > distances[0] && distances[2] > distances[0])
        return 0;

    if (distances[3] > distances[1] && distances[0] > distances[1] && distances[2] > distances[1])
        return 1;

    if (distances[3] > distances[2] && distances[0] > distances[2] && distances[1] > distances[2])
        return 2;

    return 3;
}

int32_t VanillaNetherBiomeSource::pickBiomeId(int32_t x, int32_t, int32_t z) const {
    // the cells of the zoom sit two blocks off the climate grid
    const int32_t gridX = (x - 2) >> 2;
    const int32_t gridZ = (z - 2) >> 2;

    float cornerX[4];
    float cornerZ[4];
    for (int32_t i = 0; i < 4; i++) {
        _jitter(gridX + (i & 1), gridZ + (i >> 1), cornerX[i], cornerZ[i]);
        cornerX[i] += (float) (i & 1) * CORNER_SPACING;
        cornerZ[i] += (float) (i >> 1) * CORNER_SPACING;
    }

    const int32_t corner = _nearestCorner(cornerX, cornerZ, (x - 2) & 3, (z - 2) & 3);
    return _pickClimateBiome(gridX + (corner & 1), gridZ + (corner >> 1));
}

void VanillaNetherBiomeSource::pickChunk(int32_t chunkX, int32_t chunkZ, int32_t *biomes) const {
    const int32_t GRID_SIZE = 6;
    const int32_t originX = chunkX * 4 - 1;
    const int32_t originZ = chunkZ * 4 - 1;

    int32_t climate[GRID_SIZE][GRID_SIZE];
    float jitterX[GRID_SIZE][GRID_SIZE];
    float jitterZ[GRID_SIZE][GRID_SIZE];
    for (int32_t x = 0; x < GRID_SIZE; x++) {
        for (int32_t z = 0; z < GRID_SIZE; z++) {
            climate[x][z] = _pickClimateBiome(originX + x, originZ + z);
            _jitter(originX + x, originZ + z, jitterX[x][z], jitterZ[x][z]);
        }
    }

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t cellX = ((x + chunkX * 16 - 2) >> 2) - originX;
            const int32_t cellZ = ((z + chunkZ * 16 - 2) >> 2) - originZ;

            float cornerX[4];
            float cornerZ[4];
            for (int32_t i = 0; i < 4; i++) {
                cornerX[i] = jitterX[cellX + (i & 1)][cellZ + (i >> 1)] + (float) (i & 1) * CORNER_SPACING;
                cornerZ[i] = jitterZ[cellX + (i & 1)][cellZ + (i >> 1)] + (float) (i >> 1) * CORNER_SPACING;
            }

            const int32_t corner = _nearestCorner(cornerX, cornerZ, (x - 2) & 3, (z - 2) & 3);
            biomes[(x << 4) | z] = climate[cellX + (corner & 1)][cellZ + (corner >> 1)];
        }
    }
}
