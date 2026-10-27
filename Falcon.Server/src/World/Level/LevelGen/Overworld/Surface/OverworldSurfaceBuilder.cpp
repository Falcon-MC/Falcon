#include "World/Level/LevelGen/Overworld/Surface/OverworldSurfaceBuilder.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <vector>

namespace {
    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    bool isWaterState(const BlockState &state) {
        return state.getHash() == waterState().getHash();
    }

    float remap(float input, float inMin, float inMax, float outMin, float outMax) {
        return outMin + ((input - inMin) / (inMax - inMin) * (outMax - outMin));
    }

    float remapFromNormalized(float input, float outMin, float outMax) {
        return remap(input, -1.0f, 1.0f, outMin, outMax);
    }
}

OverworldSurfaceBuilder::OverworldSurfaceBuilder(IRandom &random) {
    const IRandomPtr source = random.identical();
    mNoise = std::make_shared<NormalNoise>(*source, -6, std::vector<float>{1.0f, 1.0f, 1.0f});
}

void OverworldSurfaceBuilder::apply(LevelChunk &chunk) const {
    const int32_t chunkBaseX = chunk.getX() << 4;
    const int32_t chunkBaseZ = chunk.getZ() << 4;

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = chunk.getHeight(x, z);
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                continue;

            const BlockState topBlockState = chunk.getBlock(x, y, z);
            const SurfaceBuilderComponent *component =
                    SurfaceBuilderComponent::getForBiome((int32_t) chunk.getBiomeAt(x, y, z));
            if (component == nullptr)
                continue;

            const SurfaceMaterialAttributes &material = component->mSurfaceMaterial;
            const BlockState *topBlock = material.mTopBlock;
            const BlockState *midBlock = material.mMidBlock;
            const BlockState *seaFloorBlock = material.mSeaFloorBlock;

            if (!component->mSurfaceMaterialAdjustments.empty()) {
                const float random = mNoise->getValue(chunkBaseX + x, 0.0, chunkBaseZ + z);
                for (const SurfaceMaterialAdjustmentAttributes &element: component->mSurfaceMaterialAdjustments) {
                    if (random >= element.mNoiseUpperBound || random <= element.mNoiseLowerBound)
                        continue;

                    if (element.mTopBlock != nullptr)
                        topBlock = element.mTopBlock;

                    if (element.mMidBlock != nullptr)
                        midBlock = element.mMidBlock;

                    if (element.mSeaFloorBlock != nullptr)
                        seaFloorBlock = element.mSeaFloorBlock;
                }
            }

            if (!isWaterState(topBlockState)) {
                chunk.setBlock(x, y, z, *topBlock);

                const float midDepth = remapFromNormalized(mNoise->getValue(x, 0.0, z), 1.0f, 4.0f);
                for (int32_t i = 1; i < midDepth; i++) {
                    const int32_t midY = y - i;
                    if (midY < LevelChunk::MIN_Y)
                        break;

                    chunk.setBlock(x, midY, z, *midBlock);
                }
            } else {
                int32_t depth = 0;
                BlockState currentState = topBlockState;
                while (isWaterState(currentState)) {
                    depth++;
                    if (y - depth < LevelChunk::MIN_Y)
                        break;

                    currentState = chunk.getBlock(x, y - depth, z);
                }

                if (y - depth < LevelChunk::MIN_Y)
                    continue;

                if (depth > material.mSeaFloorDepth) {
                    chunk.setBlock(x, y - depth, z, *seaFloorBlock);
                } else {
                    for (int32_t i = 0; i < 3; i++) {
                        const int32_t midY = (y - depth) - i;
                        if (midY < LevelChunk::MIN_Y)
                            break;

                        chunk.setBlock(x, midY, z, *midBlock);
                    }
                }
            }
        }
    }
}
