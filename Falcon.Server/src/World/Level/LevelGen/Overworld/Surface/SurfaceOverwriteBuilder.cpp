#include "World/Level/LevelGen/Overworld/Surface/SurfaceOverwriteBuilder.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cmath>

namespace {

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &coarseDirtState() {
        static const BlockState state = VanillaBlocks::COARSE_DIRT().toBlockState();
        return state;
    }

    const BlockState &grassState() {
        static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &hardenedClayState() {
        static const BlockState state = VanillaBlocks::HARDENED_CLAY().toBlockState();
        return state;
    }

    const BlockState &orangeTerracottaState() {
        static const BlockState state = VanillaBlocks::ORANGE_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &whiteTerracottaState() {
        static const BlockState state = VanillaBlocks::WHITE_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &yellowTerracottaState() {
        static const BlockState state = VanillaBlocks::YELLOW_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &brownTerracottaState() {
        static const BlockState state = VanillaBlocks::BROWN_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &redTerracottaState() {
        static const BlockState state = VanillaBlocks::RED_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &lightGrayTerracottaState() {
        static const BlockState state = VanillaBlocks::LIGHT_GRAY_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &snowLayerState() {
        static const BlockState state = VanillaBlocks::SNOW_LAYER().toBlockState();
        return state;
    }

    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

}

SurfaceOverwriteBuilder::SurfaceOverwriteBuilder(IRandom &random, const XoroshiroPositionalFactory &noiseFactory,
                                                 int64_t levelSeed) {
    const std::vector<float> threeOctaves = {1.0f, 1.0f, 1.0f};
    const std::vector<float> oneOctave = {1.0f};
    const std::vector<float> fourOctaves = {1.0f, 1.0f, 1.0f, 1.0f};

    mSurfaceNoise = NormalNoise::create(noiseFactory, "minecraft:surface", -6, threeOctaves);

    // no named stream for the swamp noise in the game: old seeding kept
    IRandomPtr swampSource = random.identical();
    mSwampNoise = std::make_shared<NormalNoise>(*swampSource, -2, oneOctave);

    mClayBandsOffsetNoise = NormalNoise::create(noiseFactory, "minecraft:clay_bands_offset", -8, oneOctave);
    mBadlandsPillarNoise = NormalNoise::create(noiseFactory, "minecraft:badlands_pillar", -2, fourOctaves);
    mBadlandsPillarRoofNoise = NormalNoise::create(noiseFactory, "minecraft:badlands_pillar_roof", -8, oneOctave);
    mBadlandsSurfaceNoise = NormalNoise::create(noiseFactory, "minecraft:badlands_surface", -6, threeOctaves);

    mClayBandsCache.assign((size_t) CLAY_BANDS_SIZE, hardenedClayState());
    _generateBands(levelSeed);
}

void SurfaceOverwriteBuilder::apply(LevelChunk &chunk) const {
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t lx = x + (chunk.getX() << 4);
            const int32_t lz = z + (chunk.getZ() << 4);
            const int32_t y = chunk.getHeight(x, z);
            const int32_t biomeId = (int32_t) chunk.getColumnBiome(x, z);

            switch (biomeId) {
                case BiomeIds::MESA:
                case BiomeIds::MESA_BRYCE:
                case BiomeIds::MESA_PLATEAU_STONE:
                    _applyClayBandsDepth(chunk, x, z, lx, lz, y);
                    break;
                default:
                    break;
            }

            switch (biomeId) {
                case BiomeIds::SWAMPLAND: {
                    if (mSwampNoise->getValue((double) lx, (double) y, (double) lz) > 0.0f) {
                        if (y == SEA_LEVEL - 1)
                            chunk.setBlock(x, y, z, waterState());
                    }
                    break;
                }
                case BiomeIds::MANGROVE_SWAMP: {
                    if (mSwampNoise->getValue((double) lx, (double) y, (double) lz) > 0.0f) {
                        if (y >= SEA_LEVEL - 2 && y < SEA_LEVEL)
                            chunk.setBlock(x, y, z, waterState());
                    }
                    break;
                }
                case BiomeIds::MESA_PLATEAU_STONE: {
                    if (y > 97) {
                        const float noise = mSurfaceNoise->getValue((double) ((float) lx * 0.25f), (double) y,
                                                                    (double) ((float) lz * 0.25f));
                        if (_isInRange(noise))
                            chunk.setBlock(x, y, z, coarseDirtState());
                        else
                            chunk.setBlock(x, y, z, grassState());
                    }
                    break;
                }
                case BiomeIds::ICE_PLAINS: {
                    if (_isSolidState(chunk.getBlock(x, y, z))) {
                        const BlockState above = chunk.getBlock(x, y + 1, z);
                        if (above == airState())
                            chunk.setBlock(x, y + 1, z, snowLayerState());
                        else
                            chunk.setBlock(x, y + 1, z, 1, snowLayerState());
                    }
                    break;
                }
                case BiomeIds::MESA_BRYCE:
                    _erodedBadlandsExtension(chunk, x, z, lx, lz, y);
                    break;
                default:
                    break;
            }
        }
    }
}

void SurfaceOverwriteBuilder::_erodedBadlandsExtension(LevelChunk &chunk, int32_t localX, int32_t localZ,
                                                       int32_t worldX, int32_t worldZ, int32_t height) const {
    const double surfaceTerm = std::fabs(
            mBadlandsSurfaceNoise->getValue((double) worldX, 0.0, (double) worldZ) * 8.25);
    const double pillarTerm = mBadlandsPillarNoise->getValue(worldX * 0.2, 0.0, worldZ * 0.2) * 15.0;
    const double pillarBuffer = surfaceTerm < pillarTerm ? surfaceTerm : pillarTerm;
    if (pillarBuffer <= 0.0)
        return;

    const double pillarFloor = std::fabs(
            mBadlandsPillarRoofNoise->getValue(worldX * 0.75, 0.0, worldZ * 0.75) * 1.5);
    const double bodyTerm = pillarBuffer * pillarBuffer * 2.5;
    const double roofTerm = std::ceil(pillarFloor * 50.0) + 24.0;
    const double extensionTop = 64.0 + (bodyTerm < roofTerm ? bodyTerm : roofTerm);
    const int32_t startY = (int32_t) std::floor(extensionTop);
    if (height > startY)
        return;

    for (int32_t y = startY; y >= LevelChunk::MIN_Y; y--) {
        const BlockState state = chunk.getBlock(localX, y, localZ);
        if (_isWaterState(state))
            return;

        if (_isSolidState(state))
            break;
    }

    for (int32_t y = startY; y >= LevelChunk::MIN_Y; y--) {
        const BlockState state = chunk.getBlock(localX, y, localZ);
        if (!(state == airState()))
            break;

        chunk.setBlock(localX, y, localZ, _getClayBand(worldX, y, worldZ));
    }
}

void SurfaceOverwriteBuilder::_applyClayBandsDepth(LevelChunk &chunk, int32_t localX, int32_t localZ, int32_t worldX,
                                                   int32_t worldZ, int32_t surfaceY) const {
    if (surfaceY >= 256) {
        chunk.setBlock(localX, surfaceY, localZ, orangeTerracottaState());
    } else if (surfaceY >= 74) {
        chunk.setBlock(localX, surfaceY, localZ, _getClayBand(worldX, surfaceY, worldZ));
    }

    for (int32_t y = 74; y > 63; y--) {
        if (chunk.getBlock(localX, y, localZ) == airState())
            continue;

        if (surfaceY > 63 && surfaceY < 74)
            chunk.setBlock(localX, y, localZ, orangeTerracottaState());
        else
            chunk.setBlock(localX, y, localZ, _getClayBand(worldX, y, worldZ));
    }

    const float noise = mSurfaceNoise->getValue((double) ((float) worldX * 0.25f), 0.0,
                                                (double) ((float) worldZ * 0.25f));
    const int32_t depth = 4 + (int32_t) std::floor(std::fabs((double) noise) * 3.0);
    const int32_t minY = LevelChunk::MIN_Y > surfaceY - depth ? LevelChunk::MIN_Y : surfaceY - depth;
    for (int32_t y = surfaceY - 1; y >= minY; y--) {
        const BlockState state = chunk.getBlock(localX, y, localZ);
        if (state == airState() || _isWaterState(state))
            break;

        if (!_isSolidState(state))
            break;

        chunk.setBlock(localX, y, localZ, _getClayBand(worldX, y, worldZ));
    }
}

const BlockState &SurfaceOverwriteBuilder::_getClayBand(int32_t worldX, int32_t y, int32_t worldZ) const {
    const float raw = mClayBandsOffsetNoise->getValue((double) worldX, 0.0, (double) worldZ) * 4.0f;
    const int32_t offset = (int32_t) std::floor(raw + 0.5f);
    const int32_t size = (int32_t) mClayBandsCache.size();
    return mClayBandsCache[(size_t) ((y + offset + size) % size)];
}

void SurfaceOverwriteBuilder::_generateBands(int64_t seed) {
    SimpleRandom random(seed ^ (int64_t) _javaStringHash("clay_bands"));
    const int32_t size = (int32_t) mClayBandsCache.size();
    for (int32_t i = 0; i < size; i++)
        mClayBandsCache[(size_t) i] = hardenedClayState();

    for (int32_t i = 0; i < size; i++) {
        i += random.nextBoundedInt(5) + 1;
        if (i < size)
            mClayBandsCache[(size_t) i] = orangeTerracottaState();
    }

    _makeBands(random, 1, yellowTerracottaState());
    _makeBands(random, 2, brownTerracottaState());
    _makeBands(random, 1, redTerracottaState());

    const int32_t whiteBandCount = random.nextInt(9, 15);
    int32_t count = 0;
    for (int32_t start = 0; count < whiteBandCount && start < size; start += random.nextBoundedInt(16) + 4) {
        mClayBandsCache[(size_t) start] = whiteTerracottaState();
        if (start - 1 > 0 && random.nextBoolean())
            mClayBandsCache[(size_t) (start - 1)] = lightGrayTerracottaState();

        if (start + 1 < size && random.nextBoolean())
            mClayBandsCache[(size_t) (start + 1)] = lightGrayTerracottaState();

        count++;
    }
}

void SurfaceOverwriteBuilder::_makeBands(IRandom &random, int32_t baseWidth, const BlockState &state) {
    const int32_t size = (int32_t) mClayBandsCache.size();
    const int32_t bandCount = random.nextInt(6, 15);
    for (int32_t i = 0; i < bandCount; i++) {
        const int32_t width = baseWidth + random.nextBoundedInt(3);
        const int32_t start = random.nextBoundedInt(size - 1);
        for (int32_t p = 0; start + p < size && p < width; p++)
            mClayBandsCache[(size_t) (start + p)] = state;
    }
}

bool SurfaceOverwriteBuilder::_isWaterState(const BlockState &state) {
    return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water";
}

bool SurfaceOverwriteBuilder::_isSolidState(const BlockState &state) {
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data != nullptr && data->mSolid;
}

bool SurfaceOverwriteBuilder::_isInRange(float noise) {
    return ((double) noise >= -0.909 && (double) noise <= -0.5454)
           || ((double) noise >= -0.1818 && (double) noise <= 0.1818)
           || ((double) noise >= 0.5454 && (double) noise <= 0.909);
}

int32_t SurfaceOverwriteBuilder::_javaStringHash(const char *text) {
    uint32_t hash = 0;
    for (const char *cursor = text; *cursor != '\0'; cursor++)
        hash = hash * 31u + (uint32_t) (unsigned char) *cursor;

    return (int32_t) hash;
}
