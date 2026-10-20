#include "World/Level/LevelGen/Overworld/Feature/Decoration/ClayWithDripleavesPatchFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <memory>
#include <tuple>

namespace {

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    const char *CARDINAL_DIRECTIONS[4] = {"south", "west", "north", "east"};

    const BlockState &clayState() {
        static const BlockState state = VanillaBlocks::CLAY().toBlockState();
        return state;
    }

    const BlockState &stillWaterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const SimplexF *clayNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getRandomClayWithDripleavesSnapToFloor();
    }

    float remapFromNormalized(float input, float outMin, float outMax) {
        return outMin + ((input - -1.0f) / (1.0f - -1.0f) * (outMax - outMin));
    }

    double clampDouble(double value, double min, double max) {
        return value < min ? min : std::min(value, max);
    }

}

const char *ClayWithDripleavesPatchFeature::name() const {
    return "minecraft:random_clay_with_dripleaves_snap_to_floor_feature";
}

std::vector<int32_t> ClayWithDripleavesPatchFeature::_getHighestWorkableBlocks(LevelChunk &chunk, int32_t x,
                                                                               int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; --y) {
        if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::LUSH_CAVES)
            continue;

        const std::string &id = chunk.getBlock(x, y, z).mName;
        if ((id == "minecraft:stone" || id == "minecraft:deepslate" || id == "minecraft:clay")
            && DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
            blockYs.push_back(y);
    }

    return blockYs;
}

void ClayWithDripleavesPatchFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    const SimplexF *noisePtr = clayNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;

    BlockManager manager(level);
    std::vector<Position> placed;

    for (int32_t x = 0; x < 16; x++) {
        const int32_t baseX = (chunkX << 4) + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t baseZ = (chunkZ << 4) + z;
            if (noise.noise2D((float) baseX * 0.25f, (float) baseZ * 0.25f, true) <= 0.3)
                continue;

            for (int32_t y: _getHighestWorkableBlocks(chunk, x, z)) {
                const int32_t depth = (int32_t) clampDouble(
                        (double) remapFromNormalized(noise.noise3D((float) baseX, (float) y, (float) baseZ, true),
                                                     3.0f, 5.0f),
                        3.0, 4.0);

                for (int32_t i = 0; i < depth; i++) {
                    if ((int32_t) chunk.getBiomeAt(x, i, z) != BiomeIds::LUSH_CAVES)
                        continue;

                    manager.setBlockStateAt(baseX, y - i, baseZ, clayState());
                    placed.emplace_back(baseX, y - i, baseZ);
                }
            }
        }
    }

    const std::vector<Position> clayBlocks = placed;
    for (const Position &position: clayBlocks) {
        const int32_t x = std::get<0>(position);
        const int32_t y = std::get<1>(position);
        const int32_t z = std::get<2>(position);

        if (!DecorationSupport::isSolid(manager.getBlockAt(x, y, z)))
            continue;

        if (!DecorationSupport::isAir(manager.getBlockAt(x, y + 1, z)))
            continue;

        if (noise.noise3D((float) x, (float) y, (float) z, true) <= 0.1f)
            continue;

        bool clay = true;
        for (const DecorationSupport::FaceOffset &face: DecorationSupport::HORIZONTALS) {
            if (!manager.isCached(x + face.mX, y + face.mY, z + face.mZ)) {
                clay = false;
                break;
            }
        }

        if (clay) {
            manager.setBlockStateAt(x, y, z, stillWaterState());
            placed.emplace_back(x, y, z);
        }
    }

    const std::vector<Position> allBlocks = placed;
    for (const Position &position: allBlocks) {
        const int32_t x = std::get<0>(position);
        const int32_t y = std::get<1>(position);
        const int32_t z = std::get<2>(position);

        if (!DecorationSupport::canBeReplaced(manager.getBlockAt(x, y + 1, z)))
            continue;

        if (mRandom.nextFloat() >= 0.01f)
            continue;

        const char *direction = CARDINAL_DIRECTIONS[mRandom.nextInt(3)];
        const bool isWaterHere = DecorationSupport::isWater(manager.getBlockAt(x, y, z));

        if (mRandom.nextInt(9) < 8) {
            BlockState head = DecorationSupport::withState(VanillaBlocks::BIG_DRIPLEAF().toBlockState(),
                                                           "minecraft:cardinal_direction", direction);
            head = DecorationSupport::withByteState(head, "big_dripleaf_head", true);
            head = DecorationSupport::withState(head, "big_dripleaf_tilt", "none");

            const BlockState stem = DecorationSupport::withState(VanillaBlocks::BIG_DRIPLEAF().toBlockState(),
                                                                 "minecraft:cardinal_direction", direction);

            const int32_t size = mRandom.nextInt(isWaterHere ? 1 : 0, 4);
            for (int32_t i = 0; i < size; i++) {
                const BlockState current = manager.getBlockAt(x, y + i, z);
                if (DecorationSupport::isWater(current) || DecorationSupport::isAir(current))
                    manager.setBlockStateAt(x, y + i, z, stem);
            }

            if (!manager.isCached(x, y + size, z))
                manager.setBlockStateAt(x, y + size, z, head);

            continue;
        }

        BlockState head = DecorationSupport::withState(VanillaBlocks::SMALL_DRIPLEAF_BLOCK().toBlockState(),
                                                       "minecraft:cardinal_direction", direction);
        head = DecorationSupport::withByteState(head, "upper_block_bit", true);

        const BlockState stem = DecorationSupport::withState(VanillaBlocks::SMALL_DRIPLEAF_BLOCK().toBlockState(),
                                                             "minecraft:cardinal_direction", direction);

        int32_t offset = 1;
        if (isWaterHere)
            offset = 0;

        manager.setBlockStateAt(x, y + offset, z, stem);
        manager.setBlockStateAt(x, y + 1 + offset, z, head);
    }

    queueObject(manager);
}
