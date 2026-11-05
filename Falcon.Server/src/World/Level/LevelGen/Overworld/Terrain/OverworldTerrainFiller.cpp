#include "World/Level/LevelGen/Overworld/Terrain/OverworldTerrainFiller.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"

#include <cmath>
#include <limits>

namespace {

    constexpr int32_t MIN_Y = OverworldGeneratorConstants::MIN_Y;
    constexpr int32_t MIN_CELL_Y = MIN_Y / 8;
    constexpr int32_t CELL_COUNT_Y = 40;
    constexpr int32_t SEA_LEVEL = OverworldGeneratorConstants::SEA_LEVEL;
    // densities below this skip the caves and keep the default corner noises (sub_14CDEE380)
    constexpr float SKIP_DENSITY = -150.0f;
    constexpr float CAVE_SCALE = 128.0f;
    // floats 0x4108E1E5 and 0x4088E1E5: 684.412 / 80 and 684.412 / 160
    constexpr float MAIN_NOISE_XZ_SCALE = 8.55515f;
    constexpr float MAIN_NOISE_Y_SCALE = 4.277575f;
    constexpr float LIMIT_NOISE_SCALE = 684.412f;

    struct VeinType {
        const BlockState *mOre;
        const BlockState *mRawOre;
        const BlockState *mFiller;
        int32_t mMinY;
        int32_t mMaxY;
    };

    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

    const BlockState &deepslateState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE().toBlockState();
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    // vein types as the game builds them (sub_14CDB0800)
    const VeinType &copperVein() {
        static const BlockState ore = VanillaBlocks::COPPER_ORE().toBlockState();
        static const BlockState raw = VanillaBlocks::RAW_COPPER_BLOCK().toBlockState();
        static const BlockState filler = VanillaBlocks::GRANITE().toBlockState();
        static const VeinType type{&ore, &raw, &filler, 0, 50};
        return type;
    }

    const VeinType &ironVein() {
        static const BlockState ore = VanillaBlocks::DEEPSLATE_IRON_ORE().toBlockState();
        static const BlockState raw = VanillaBlocks::RAW_IRON_BLOCK().toBlockState();
        static const BlockState filler = VanillaBlocks::TUFF().toBlockState();
        static const VeinType type{&ore, &raw, &filler, -60, -8};
        return type;
    }

    float clampUnit(float value) {
        return value > 1.0f ? 1.0f : std::fmax(-1.0f, value);
    }

    float halfMapped(float value) {
        return (value + 1.0f) * 0.5f;
    }

    std::vector<LatticeNoise> createOctaves(XoroshiroRandom &random, int32_t count) {
        std::vector<LatticeNoise> octaves;
        octaves.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; i++)
            octaves.emplace_back(random);

        return octaves;
    }

    XoroshiroPositionalFactory forkedFactory(const XoroshiroPositionalFactory &noiseFactory, const char *name) {
        XoroshiroRandom random = noiseFactory.hashOf(name);
        return random.forkPositional();
    }

    int32_t cornerIndex(int32_t x, int32_t z, int32_t y) {
        return (x * 5 + z) * 41 + y;
    }

    // along z, then x, then y, like the game
    class CellInterpolator {
    public:
        void load(const std::vector<float> &values, int32_t cellX, int32_t cellZ, int32_t cellY) {
            mCorners[0] = values[(size_t) cornerIndex(cellX, cellZ, cellY)];
            mCorners[1] = values[(size_t) cornerIndex(cellX + 1, cellZ, cellY)];
            mCorners[2] = values[(size_t) cornerIndex(cellX, cellZ + 1, cellY)];
            mCorners[3] = values[(size_t) cornerIndex(cellX + 1, cellZ + 1, cellY)];
            mCorners[4] = values[(size_t) cornerIndex(cellX, cellZ, cellY + 1)];
            mCorners[5] = values[(size_t) cornerIndex(cellX + 1, cellZ, cellY + 1)];
            mCorners[6] = values[(size_t) cornerIndex(cellX, cellZ + 1, cellY + 1)];
            mCorners[7] = values[(size_t) cornerIndex(cellX + 1, cellZ + 1, cellY + 1)];
        }

        void selectZ(float t) {
            mAlongZ[0] = (mCorners[2] - mCorners[0]) * t + mCorners[0];
            mAlongZ[1] = (mCorners[3] - mCorners[1]) * t + mCorners[1];
            mAlongZ[2] = (mCorners[6] - mCorners[4]) * t + mCorners[4];
            mAlongZ[3] = (mCorners[7] - mCorners[5]) * t + mCorners[5];
        }

        void selectX(float t) {
            mAlongX[0] = (mAlongZ[1] - mAlongZ[0]) * t + mAlongZ[0];
            mAlongX[1] = (mAlongZ[3] - mAlongZ[2]) * t + mAlongZ[2];
        }

        void selectY(float t) {
            mValue = (mAlongX[1] - mAlongX[0]) * t + mAlongX[0];
        }

        float value() const {
            return mValue;
        }

    private:
        float mCorners[8] = {};
        float mAlongZ[4] = {};
        float mAlongX[2] = {};
        float mValue = 0.0f;
    };

}

OverworldTerrainFiller::OverworldTerrainFiller(const XoroshiroPositionalFactory &noiseFactory,
                                               const OverworldClimate &climate)
        : mClimate(climate),
          mVeinFactory(forkedFactory(noiseFactory, "minecraft:ore")),
          mDeepslateFactory(forkedFactory(noiseFactory, "minecraft:depth_based_layer")),
          mAquiferNoises(noiseFactory) {
    // one stream for the three stacks, in this order (sub_145C2AAB0)
    XoroshiroRandom terrain = noiseFactory.hashOf("minecraft:terrain");
    mMinLimitNoise = createOctaves(terrain, 16);
    mMaxLimitNoise = createOctaves(terrain, 16);
    mMainNoise = createOctaves(terrain, 8);

    mPillar = NormalNoise::create(noiseFactory, "minecraft:pillar", -7, {1.0f, 1.0f});
    mPillarRareness = NormalNoise::create(noiseFactory, "minecraft:pillar_rareness", -8, {1.0f});
    mPillarThickness = NormalNoise::create(noiseFactory, "minecraft:pillar_thickness", -8, {1.0f});
    mSpaghetti2d = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d", -7, {1.0f});
    mSpaghetti2dElevation = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_elevation", -8, {1.0f});
    mSpaghetti2dModulator = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_modulator", -11, {1.0f});
    mSpaghetti2dThickness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_thickness", -11, {1.0f});
    mSpaghetti3dFirst = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_1", -7, {1.0f});
    mSpaghetti3dSecond = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_2", -7, {1.0f});
    mSpaghetti3dRarity = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_rarity", -11, {1.0f});
    mSpaghetti3dThickness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_thickness", -8, {1.0f});
    mSpaghettiRoughness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_roughness", -5, {1.0f});
    mSpaghettiRoughnessModulator = NormalNoise::create(noiseFactory, "minecraft:spaghetti_roughness_modulator", -8,
                                                       {1.0f});
    mCaveEntrance = NormalNoise::create(noiseFactory, "minecraft:cave_entrance", -7, {0.4f, 0.5f, 1.0f});
    mCaveLayer = NormalNoise::create(noiseFactory, "minecraft:cave_layer", -8, {1.0f});
    mCaveCheese = NormalNoise::create(noiseFactory, "minecraft:cave_cheese", -8,
                                      {0.5f, 1.0f, 2.0f, 1.0f, 2.0f, 1.0f, 0.0f, 2.0f, 0.0f});

    mNoodle = NormalNoise::create(noiseFactory, "minecraft:noodle", -8, {1.0f});
    mNoodleThickness = NormalNoise::create(noiseFactory, "minecraft:noodle_thickness", -8, {1.0f});
    mNoodleRidgeA = NormalNoise::create(noiseFactory, "minecraft:noodle_ridge_a", -7, {1.0f});
    mNoodleRidgeB = NormalNoise::create(noiseFactory, "minecraft:noodle_ridge_b", -7, {1.0f});

    mVeininess = NormalNoise::create(noiseFactory, "minecraft:ore_veininess", -8, {1.0f});
    mVeinA = NormalNoise::create(noiseFactory, "minecraft:ore_vein_a", -7, {1.0f});
    mVeinB = NormalNoise::create(noiseFactory, "minecraft:ore_vein_b", -7, {1.0f});
    mVeinGap = NormalNoise::create(noiseFactory, "minecraft:ore_gap", -5, {1.0f});
}

float OverworldTerrainFiller::_caves(float density, float x, float y, float z) const {
    const float roughnessWeight = halfMapped(mSpaghettiRoughnessModulator->getValue(x, y, z)) * 0.1f;
    const float roughness = std::fabs(mSpaghettiRoughness->getValue(x, y, z));

    const float rarityNoise = mSpaghetti3dRarity->getValue(x + x, y, z + z);
    float rarity = 0.75f;
    if (rarityNoise >= -0.5f) {
        rarity = 1.0f;
        if (rarityNoise >= 0.0f)
            rarity = rarityNoise < 0.5f ? 1.5f : 2.0f;
    }

    const float tunnelThickness = halfMapped(mSpaghetti3dThickness->getValue(x, y, z)) * 0.023000002f + 0.065f;
    const float inverseRarity = 1.0f / rarity;
    const float scaledX = inverseRarity * x;
    const float scaledY = y * inverseRarity;
    const float scaledZ = z * inverseRarity;
    const float tunnelFirst = mSpaghetti3dFirst->getValue(scaledX, scaledY, scaledZ);
    const float tunnelSecond = mSpaghetti3dSecond->getValue(scaledX, scaledY, scaledZ);

    const float entrance = mCaveEntrance->getValue(x * 0.75f, y * 0.5f, z * 0.75f) + 0.37f;
    const float entranceGradient = (y + 10.0f) / 40.0f;
    float entranceBonus = 0.3f;
    if (entranceGradient >= 0.0f) {
        entranceBonus = 0.0f;
        if (entranceGradient <= 1.0f)
            entranceBonus = 0.3f - entranceGradient * 0.3f;
    }

    const float tunnel = clampUnit(std::fmax(std::fabs(tunnelSecond * rarity) - tunnelThickness,
                                             std::fabs(tunnelFirst * rarity) - tunnelThickness));
    const float roughnessTerm = roughnessWeight * (0.4f - roughness);
    const float entrances = std::fmin(tunnel + roughnessTerm, entrance + entranceBonus);

    if (density < 200.0f)
        return std::fmin(entrances * CAVE_SCALE * 5.0f, density);

    const float cheeseNoise = mCaveCheese->getValue(x, y * 0.6666667f, z) + 0.27f;
    const float cheeseGradient = (density + -200.0f) * 1.28f;
    float cheeseBonus = 0.5f;
    if (cheeseGradient >= 0.0f) {
        cheeseBonus = 0.0f;
        if (cheeseGradient <= 1.0f)
            cheeseBonus = 0.5f - cheeseGradient * 0.5f;
    }

    const float cheese = clampUnit(cheeseNoise) + cheeseBonus;
    const float layerNoise = mCaveLayer->getValue(x, y * 8.0f, z);
    const float layer = layerNoise * layerNoise * 4.0f;

    const float modulator = mSpaghetti2dModulator->getValue(x + x, y, z + z);
    float rarity2d = 0.5f;
    if (modulator >= -0.75f) {
        rarity2d = 0.75f;
        if (modulator >= -0.5f) {
            rarity2d = 1.0f;
            if (modulator >= 0.5f)
                rarity2d = modulator < 0.75f ? 2.0f : 3.0f;
        }
    }

    const float cheeseAndLayer = cheese + layer;
    const float thicknessNoise = mSpaghetti2dThickness->getValue(x + x, y, z + z);
    const float inverseRarity2d = 1.0f / rarity2d;
    const float spaghetti = mSpaghetti2d->getValue(inverseRarity2d * x, y * inverseRarity2d, z * inverseRarity2d);

    float elevation = static_cast<float>(MIN_CELL_Y);
    const float elevationNoise = mSpaghetti2dElevation->getValue(x, 0.0f, z);
    if (std::fabs(elevation + -8.0f) > 1.1920929e-7f)
        elevation = elevation + (8.0f - elevation) * halfMapped(elevationNoise);

    const float height = std::fabs(y * -0.125f + elevation);
    const float thickness2d = halfMapped(thicknessNoise) * 0.6999999f + 0.6f;
    const float distance = height - thickness2d;
    const float spaghetti2d = clampUnit(std::fmax(distance * distance * distance,
                                                  thickness2d * -0.083f + std::fabs(rarity2d * spaghetti)));
    const float underground = std::fmin(cheeseAndLayer, std::fmin(entrances, roughnessTerm + spaghetti2d));

    const float rarenessNoise = mPillarRareness->getValue(x, y, z);
    const float rareness = halfMapped(rarenessNoise) + halfMapped(rarenessNoise) + 0.0f;
    const float thicknessBase = halfMapped(mPillarThickness->getValue(x, y, z)) * 1.1f + 0.0f;
    const float pillarThickness = static_cast<float>(std::pow(static_cast<double>(thicknessBase), 3.0));
    const float pillarNoise = mPillar->getValue(x * 25.0f, y * 0.3f, z * 25.0f);
    const float pillar = (pillarNoise + pillarNoise - rareness) * pillarThickness;
    const float pillars = pillar > 0.03f ? pillar : -std::numeric_limits<float>::max();

    return std::fmax(underground, pillars) * CAVE_SCALE;
}

void OverworldTerrainFiller::_sampleCorners(int32_t chunkX, int32_t chunkZ, Corners &corners) const {
    const int32_t quartX = chunkX * 4;
    const int32_t quartZ = chunkZ * 4;
    const float start[3] = {static_cast<float>(quartX), static_cast<float>(MIN_CELL_Y), static_cast<float>(quartZ)};

    std::vector<float> main((size_t) CORNER_COUNT, 0.0f);
    std::vector<float> minLimit((size_t) CORNER_COUNT, 0.0f);
    std::vector<float> maxLimit((size_t) CORNER_COUNT, 0.0f);

    float frequency = 1.0f;
    for (const LatticeNoise &octave: mMainNoise) {
        const float scale[3] = {frequency * MAIN_NOISE_XZ_SCALE, frequency * MAIN_NOISE_Y_SCALE,
                                frequency * MAIN_NOISE_XZ_SCALE};
        octave.addLattice(main.data(), start, CORNERS_XZ, CORNERS_Y, CORNERS_XZ, scale, frequency);
        frequency = frequency * 0.5f;
    }

    frequency = 1.0f;
    for (const LatticeNoise &octave: mMinLimitNoise) {
        const float scale[3] = {frequency * LIMIT_NOISE_SCALE, frequency * LIMIT_NOISE_SCALE,
                                frequency * LIMIT_NOISE_SCALE};
        octave.addLattice(minLimit.data(), start, CORNERS_XZ, CORNERS_Y, CORNERS_XZ, scale, frequency);
        frequency = frequency * 0.5f;
    }

    frequency = 1.0f;
    for (const LatticeNoise &octave: mMaxLimitNoise) {
        const float scale[3] = {frequency * LIMIT_NOISE_SCALE, frequency * LIMIT_NOISE_SCALE,
                                frequency * LIMIT_NOISE_SCALE};
        octave.addLattice(maxLimit.data(), start, CORNERS_XZ, CORNERS_Y, CORNERS_XZ, scale, frequency);
        frequency = frequency * 0.5f;
    }

    corners.mDensity.assign((size_t) CORNER_COUNT, 0.0f);
    corners.mNoodleToggle.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mNoodleThickness.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mNoodleRidgeA.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mNoodleRidgeB.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mVeinToggle.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mVeinRidgeA.assign((size_t) CORNER_COUNT, 1.0f);
    corners.mVeinRidgeB.assign((size_t) CORNER_COUNT, 1.0f);

    for (int32_t cornerX = 0; cornerX < CORNERS_XZ; cornerX++) {
        const float x = (static_cast<float>(cornerX) + static_cast<float>(quartX)) * 4.0f;
        for (int32_t cornerZ = 0; cornerZ < CORNERS_XZ; cornerZ++) {
            const float z = (static_cast<float>(cornerZ) + static_cast<float>(quartZ)) * 4.0f;
            const OverworldClimate::Column column = mClimate.sampleColumn(quartX + cornerX, quartZ + cornerZ);

            for (int32_t cornerY = 0; cornerY < CORNERS_Y; cornerY++) {
                const size_t index = (size_t) cornerIndex(cornerX, cornerZ, cornerY);
                const float y = (static_cast<float>(MIN_CELL_Y) + static_cast<float>(cornerY)) * 8.0f;

                float terrain = minLimit[index] * 0.001953125f;
                const float blend = (main[index] / 10.0f + 1.0f) * 0.5f;
                if (blend >= 0.0f) {
                    if (blend <= 1.0f)
                        terrain = terrain + (maxLimit[index] * 0.001953125f - terrain) * blend;
                    else
                        terrain = maxLimit[index] * 0.001953125f;
                }

                const float gradient = static_cast<float>(8 * (cornerY + MIN_CELL_Y)) * -0.0078125f + 1.0f;
                const float shape = (gradient + column.mOffset + column.mJaggedness) * column.mFactor;
                float density = shape * (shape > 0.0f ? 4.0f : 1.0f) + terrain;
                if (density >= SKIP_DENSITY)
                    density = _caves(density, x, y, z);

                const float topSlide = static_cast<float>(CELL_COUNT_Y - cornerY) * 0.5f;
                float slid = -10.0f;
                if (topSlide >= 0.0f) {
                    if (topSlide <= 1.0f)
                        density = (density + 10.0f) * topSlide + -10.0f;

                    slid = density;
                }

                const float bottomSlide = static_cast<float>(cornerY) / 3.0f;
                const float result = bottomSlide > 1.0f ? slid : (slid + -15.0f) * bottomSlide + 15.0f;

                if (result >= SKIP_DENSITY) {
                    corners.mVeinToggle[index] = clampUnit(mVeininess->getValue(x * 1.5f, y * 1.5f, z * 1.5f));
                    corners.mVeinRidgeA[index] = clampUnit(mVeinA->getValue(x * 4.0f, y * 4.0f, z * 4.0f));
                    corners.mVeinRidgeB[index] = clampUnit(mVeinB->getValue(x * 4.0f, y * 4.0f, z * 4.0f));
                    corners.mNoodleToggle[index] = clampUnit(mNoodle->getValue(x, y, z));
                    corners.mNoodleThickness[index] = clampUnit(mNoodleThickness->getValue(x, y, z));
                    const float ridgeX = x * 2.6666667f;
                    const float ridgeY = y * 2.6666667f;
                    const float ridgeZ = z * 2.6666667f;
                    corners.mNoodleRidgeA[index] = clampUnit(mNoodleRidgeA->getValue(ridgeX, ridgeY, ridgeZ));
                    corners.mNoodleRidgeB[index] = clampUnit(mNoodleRidgeB->getValue(ridgeX, ridgeY, ridgeZ));
                }

                corners.mDensity[index] = result;
            }
        }
    }
}

OverworldAquifer OverworldTerrainFiller::createAquifer() const {
    return OverworldAquifer(mAquiferNoises, mClimate, SEA_LEVEL);
}

void OverworldTerrainFiller::fill(LevelChunk &chunk, int32_t *heightMap, OverworldAquifer &aquifer) const {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int32_t baseX = chunkX << 4;
    const int32_t baseZ = chunkZ << 4;

    Corners corners;
    _sampleCorners(chunkX, chunkZ, corners);

    CellInterpolator density;
    CellInterpolator noodleToggle;
    CellInterpolator noodleThickness;
    CellInterpolator noodleRidgeA;
    CellInterpolator noodleRidgeB;
    CellInterpolator veinToggle;
    CellInterpolator veinRidgeA;
    CellInterpolator veinRidgeB;
    CellInterpolator *const interpolators[] = {
            &density, &noodleToggle, &noodleThickness, &noodleRidgeA, &noodleRidgeB, &veinToggle, &veinRidgeA,
            &veinRidgeB
    };
    const std::vector<float> *const sources[] = {
            &corners.mDensity, &corners.mNoodleToggle, &corners.mNoodleThickness, &corners.mNoodleRidgeA,
            &corners.mNoodleRidgeB, &corners.mVeinToggle, &corners.mVeinRidgeA, &corners.mVeinRidgeB
    };

    // cells above the preliminary surface and sea level stop at the first one with a solid bottom corner
    const int32_t seaLevelCell = (uint8_t) (SEA_LEVEL - MIN_Y) >> 3;

    for (int32_t cellX = 0; cellX < 4; cellX++) {
        for (int32_t cellZ = 0; cellZ < 4; cellZ++) {
            const int32_t surface = mClimate.preliminarySurfaceLevel(chunkX * 4 + cellX, chunkZ * 4 + cellZ);
            uint8_t lowestTop = (uint8_t) ((surface - MIN_Y) / 8);
            if ((uint8_t) seaLevelCell > lowestTop)
                lowestTop = (uint8_t) seaLevelCell;

            int32_t topCell = CELL_COUNT_Y;
            if (lowestTop <= 38) {
                do {
                    const int32_t corner = topCell - 1;
                    if (corners.mDensity[(size_t) cornerIndex(cellX, cellZ, corner)] >= 0.0f
                        || corners.mDensity[(size_t) cornerIndex(cellX, cellZ + 1, corner)] >= 0.0f
                        || corners.mDensity[(size_t) cornerIndex(cellX + 1, cellZ, corner)] >= 0.0f
                        || corners.mDensity[(size_t) cornerIndex(cellX + 1, cellZ + 1, corner)] >= 0.0f)
                        break;

                    topCell--;
                } while (lowestTop + 1 < topCell);
            }

            for (int32_t cellY = 0; cellY < topCell; cellY++) {
                for (size_t i = 0; i < 8; i++)
                    interpolators[i]->load(*sources[i], cellX, cellZ, cellY);

                for (int32_t localZ = 0; localZ < 4; localZ++) {
                    const float tz = 0.25f * static_cast<float>(localZ);
                    for (CellInterpolator *interpolator: interpolators)
                        interpolator->selectZ(tz);

                    const int32_t z = cellZ * 4 + localZ;
                    for (int32_t localX = 0; localX < 4; localX++) {
                        const float tx = 0.25f * static_cast<float>(localX);
                        for (CellInterpolator *interpolator: interpolators)
                            interpolator->selectX(tx);

                        const int32_t x = cellX * 4 + localX;
                        for (int32_t localY = 0; localY < 8; localY++) {
                            const float ty = 0.125f * static_cast<float>(localY);
                            for (CellInterpolator *interpolator: interpolators)
                                interpolator->selectY(ty);

                            const int32_t y = MIN_Y + cellY * 8 + localY;
                            const int32_t worldX = baseX + x;
                            const int32_t worldZ = baseZ + z;

                            const float scaled = clampUnit(density.value() * 0.005f);
                            float value = scaled * scaled * scaled / -24.0f + scaled * 0.5f;
                            if (y >= MIN_Y + 4 && value > 0.0f && noodleToggle.value() >= 0.0f) {
                                const float thickness = halfMapped(noodleThickness.value());
                                float width = 0.05f;
                                if (thickness >= 0.0f)
                                    width = thickness <= 1.0f ? thickness * 0.05f + 0.05f : 0.1f;

                                const float ridgeA = std::fabs(noodleRidgeA.value() * 1.5f) - width;
                                const float ridgeB = std::fabs(noodleRidgeB.value() * 1.5f) - width;
                                value = std::fmin(std::fmax(ridgeB, ridgeA), value);
                            }

                            OverworldAquifer::Substance substance;
                            if (value <= 0.0f) {
                                substance = aquifer.compute(worldX, y, worldZ);
                                value = value + substance.mDensity;
                            }

                            const BlockState *state = nullptr;
                            if (value > 0.0f) {
                                state = &stoneState();
                                if (y < 0) {
                                    state = &deepslateState();
                                } else if (y <= 8) {
                                    const float chance = 1.0f - static_cast<float>(y) * 0.125f;
                                    if (chance > mDeepslateFactory.at(worldX, y, worldZ).nextFloat())
                                        state = &deepslateState();
                                }

                                const float toggle = veinToggle.value();
                                const VeinType &vein = toggle > 0.0f ? copperVein() : ironVein();
                                const float aboveMin = static_cast<float>(y) - static_cast<float>(vein.mMinY);
                                const float belowMax = static_cast<float>(vein.mMaxY) - static_cast<float>(y);
                                if (aboveMin >= 0.0f && belowMax >= 0.0f) {
                                    const float edgeDistance = std::fmin(belowMax, aboveMin) / 20.0f;
                                    const float edge = edgeDistance > 1.0f ? 0.0f : edgeDistance * 0.2f + -0.2f;
                                    const float strength = std::fabs(toggle);
                                    if (edge + strength >= 0.4f) {
                                        XoroshiroRandom random = mVeinFactory.at(worldX, y, worldZ);
                                        if (random.nextFloat() <= 0.7f
                                            && std::fmax(std::fabs(veinRidgeA.value()) + -0.08f,
                                                         std::fabs(veinRidgeB.value()) + -0.08f) < 0.0f) {
                                            const float roll = random.nextFloat();
                                            const float richnessT = (strength + -0.4f) / 0.20000002f;
                                            float richness = 0.1f;
                                            if (richnessT >= 0.0f)
                                                richness = richnessT <= 1.0f ? richnessT * 0.20000002f + 0.1f : 0.3f;

                                            const float gap = mVeinGap->getValue(static_cast<float>(worldX),
                                                                                 static_cast<float>(y),
                                                                                 static_cast<float>(worldZ));
                                            state = vein.mFiller;
                                            if (richness > roll && (std::fmax(-1.0f, gap) > -0.3f || gap > 1.0f))
                                                state = random.nextFloat() < 0.02f ? vein.mRawOre : vein.mOre;
                                        }
                                    }
                                }
                            } else if (y < substance.mFluidLevel) {
                                state = substance.mLava ? &lavaState() : &waterState();
                            }

                            if (state == nullptr)
                                continue;

                            chunk.setBlock(x, y, z, *state);
                            if (y > heightMap[(x << 4) | z])
                                heightMap[(x << 4) | z] = y;
                        }
                    }
                }
            }
        }
    }
}
