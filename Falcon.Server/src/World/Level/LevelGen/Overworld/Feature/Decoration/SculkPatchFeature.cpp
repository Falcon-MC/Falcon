#include "World/Level/LevelGen/Overworld/Feature/Decoration/SculkPatchFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace {

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    const BlockState &sculkState() {
        static const BlockState state = VanillaBlocks::SCULK().toBlockState();
        return state;
    }

    const BlockState &sculkVeinState() {
        static const BlockState state = VanillaBlocks::SCULK_VEIN().toBlockState();
        return state;
    }

    const SimplexF *sculkNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getSculkPatch();
    }

    const int32_t FACE_INDEX[6] = {0, 1, 4, 2, 3, 5};
    const int32_t OPPOSITE_INDEX[6] = {1, 0, 2, 4, 5, 3};

    bool isSculkVein(const BlockState &state) {
        return state.mName == "minecraft:sculk_vein";
    }

    int32_t veinBits(const BlockState &state) {
        if (!isSculkVein(state))
            return 0;

        const Tag *tag = state.mStates.get("multi_face_direction_bits");
        if (tag == nullptr)
            return 0;

        return tag->asInt();
    }

}

const char *SculkPatchFeature::name() const {
    return "minecraft:sculk_patch_feature";
}

void SculkPatchFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    const SimplexF *noisePtr = sculkNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;

    BlockManager manager(level);
    std::vector<Position> placedSculk;

    for (int32_t x = 0; x < 16; x++) {
        const int32_t baseX = (chunkX << 4) + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t baseZ = (chunkZ << 4) + z;
            for (int32_t y = LevelChunk::MAX_Y; y > LevelChunk::MIN_Y; y--) {
                if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::DEEP_DARK)
                    continue;

                if (DecorationSupport::isAir(manager.getBlockAt(baseX, y, baseZ)))
                    continue;

                bool air = false;
                for (const DecorationSupport::FaceOffset &face: DecorationSupport::ALL_FACES) {
                    const int32_t sideX = baseX + face.mX;
                    const int32_t sideY = y + face.mY;
                    const int32_t sideZ = baseZ + face.mZ;
                    if ((sideX >> 4) != chunkX || (sideZ >> 4) != chunkZ)
                        continue;

                    if (DecorationSupport::isAir(manager.getBlockAt(sideX, sideY, sideZ))) {
                        air = true;
                        break;
                    }
                }

                if (!air)
                    continue;

                if (noise.noise3D((float) baseX * 5.5f, (float) y * 5.5f, (float) baseZ * 5.5f, true) < 0.1f) {
                    manager.setBlockStateAt(baseX, y, baseZ, sculkState());
                    placedSculk.emplace_back(baseX, y, baseZ);
                }
            }
        }
    }

    std::map<Position, int32_t> veinsToPlace;

    for (const Position &sculkPos: placedSculk) {
        const int32_t sculkX = std::get<0>(sculkPos);
        const int32_t sculkY = std::get<1>(sculkPos);
        const int32_t sculkZ = std::get<2>(sculkPos);

        for (int32_t faceIndex = 0; faceIndex < 6; faceIndex++) {
            const DecorationSupport::FaceOffset &face = DecorationSupport::ALL_FACES[faceIndex];
            const int32_t neighborX = sculkX + face.mX;
            const int32_t neighborY = sculkY + face.mY;
            const int32_t neighborZ = sculkZ + face.mZ;
            if (neighborY <= LevelChunk::MIN_Y)
                continue;

            if ((neighborX >> 4) != (sculkX >> 4) || (neighborZ >> 4) != (sculkZ >> 4))
                continue;

            const BlockState neighbor = manager.getBlockAt(neighborX, neighborY, neighborZ);
            const bool freeSpot = DecorationSupport::isAir(neighbor) || isSculkVein(neighbor);
            const bool carrierBlock = DecorationSupport::isSolid(neighbor)
                                      && neighbor.mName != "minecraft:sculk"
                                      && !isSculkVein(neighbor);

            if (freeSpot) {
                int32_t bits = veinBits(neighbor);

                for (int32_t supportIndex = 0; supportIndex < 6; supportIndex++) {
                    const DecorationSupport::FaceOffset &supportFace = DecorationSupport::ALL_FACES[supportIndex];
                    const BlockState support = manager.getBlockAt(neighborX + supportFace.mX,
                                                                  neighborY + supportFace.mY,
                                                                  neighborZ + supportFace.mZ);
                    if (!DecorationSupport::isSolid(support)
                        || support.mName == "minecraft:sculk"
                        || isSculkVein(support))
                        continue;

                    bits |= 1 << FACE_INDEX[supportIndex];
                }

                if (bits != 0) {
                    const Position key(neighborX, neighborY, neighborZ);
                    veinsToPlace[key] = veinsToPlace.count(key) == 0 ? bits : (veinsToPlace[key] | bits);
                }
            }

            if (carrierBlock) {
                for (int32_t outerIndex = 0; outerIndex < 6; outerIndex++) {
                    const DecorationSupport::FaceOffset &outerFace = DecorationSupport::ALL_FACES[outerIndex];
                    const int32_t veinX = neighborX + outerFace.mX;
                    const int32_t veinY = neighborY + outerFace.mY;
                    const int32_t veinZ = neighborZ + outerFace.mZ;

                    if ((veinX >> 4) != (sculkX >> 4) || (veinZ >> 4) != (sculkZ >> 4))
                        continue;

                    const BlockState existing = manager.getBlockAt(veinX, veinY, veinZ);
                    if (!DecorationSupport::isAir(existing) && !isSculkVein(existing))
                        continue;

                    int32_t bits = veinBits(existing);
                    bits |= 1 << OPPOSITE_INDEX[outerIndex];

                    const Position key(veinX, veinY, veinZ);
                    veinsToPlace[key] = veinsToPlace.count(key) == 0 ? bits : (veinsToPlace[key] | bits);
                }
            }
        }
    }

    for (const std::pair<const Position, int32_t> &entry: veinsToPlace) {
        const int32_t veinX = std::get<0>(entry.first);
        const int32_t veinY = std::get<1>(entry.first);
        const int32_t veinZ = std::get<2>(entry.first);

        const BlockState existing = manager.getBlockAt(veinX, veinY, veinZ);
        const int32_t mergedBits = veinBits(existing) | entry.second;
        if (mergedBits == 0)
            continue;

        manager.setBlockStateAt(veinX, veinY, veinZ,
                                DecorationSupport::withState(sculkVeinState(), "multi_face_direction_bits",
                                                             mergedBits));
    }

    queueObject(manager);
}
