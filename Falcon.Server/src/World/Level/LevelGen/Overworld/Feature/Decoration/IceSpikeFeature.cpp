#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceSpikeFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

    const BlockState &spikeState() {
        static const BlockState state = VanillaBlocks::PACKED_ICE().toBlockState();
        return state;
    }

    const BlockState &snowLayerState() {
        static const BlockState state = VanillaBlocks::SNOW_LAYER().toBlockState();
        return state;
    }

    bool canPlaceOn(const std::string &id) {
        return id == "minecraft:snow"
               || id == "minecraft:snow_layer"
               || id == "minecraft:dirt"
               || id == "minecraft:grass_block"
               || id == "minecraft:ice"
               || id == "minecraft:packed_ice";
    }

    bool canReplace(const std::string &id) {
        return id == "minecraft:snow"
               || id == "minecraft:snow_layer"
               || id == "minecraft:ice"
               || id == "minecraft:air"
               || id == "minecraft:dirt"
               || id == "minecraft:grass_block";
    }

}

const char *IceSpikeFeature::name() const {
    return "minecraft:ice_spike_feature";
}

void IceSpikeFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t attempts = 3 + mRandom.nextInt(3);

    for (int32_t attempt = 0; attempt < attempts; attempt++) {
        const int32_t originX = (chunkX << 4) + mRandom.nextInt(16);
        const int32_t originZ = (chunkZ << 4) + mRandom.nextInt(16);
        int32_t originY = chunk.getHeight(originX & 15, originZ & 15);

        while (DecorationSupport::isAir(manager.getBlockAt(originX, originY, originZ))
               && originY > LevelChunk::MIN_Y + 2)
            originY--;

        if (DecorationSupport::getBiomeAt(level, originX, originY, originZ) != BiomeIds::ICE_PLAINS_SPIKES)
            continue;

        if (!canPlaceOn(manager.getBlockAt(originX, originY, originZ).mName))
            continue;

        const int32_t height = mRandom.nextInt(4) + 7;
        const int32_t width = height / 4 + mRandom.nextInt(2);
        if (width > 1 && mRandom.nextInt(60) == 0)
            originY += 10 + mRandom.nextInt(30);

        for (int32_t yOff = 0; yOff < height; yOff++) {
            const float scale = (1.0f - (float) yOff / (float) height) * (float) width;
            const int32_t newWidth = (int32_t) std::ceil(scale);

            for (int32_t xo = -newWidth; xo <= newWidth; xo++) {
                const float dx = (float) std::abs(xo) - 0.25f;
                for (int32_t zo = -newWidth; zo <= newWidth; zo++) {
                    const float dz = (float) std::abs(zo) - 0.25f;
                    if (!((xo == 0 && zo == 0) || dx * dx + dz * dz <= scale * scale))
                        continue;

                    if (!((xo != -newWidth && xo != newWidth && zo != -newWidth && zo != newWidth)
                          || mRandom.nextFloat() <= 0.75f))
                        continue;

                    const int32_t px = originX + xo;
                    const int32_t py = originY + yOff;
                    const int32_t pz = originZ + zo;
                    std::string state = manager.getBlockAt(px, py, pz).mName;
                    if (state == "minecraft:air" || canReplace(state))
                        manager.setBlockStateAt(px, py, pz, spikeState());

                    if (yOff != 0 && newWidth > 1) {
                        const int32_t nx = originX + xo;
                        const int32_t ny = originY - yOff;
                        const int32_t nz = originZ + zo;
                        state = manager.getBlockAt(nx, ny, nz).mName;
                        if (state == "minecraft:air" || canReplace(state))
                            manager.setBlockStateAt(nx, ny, nz, spikeState());
                    }
                }
            }
        }

        int32_t pillarWidth = width - 1;
        if (pillarWidth < 0)
            pillarWidth = 0;
        else if (pillarWidth > 1)
            pillarWidth = 1;

        for (int32_t xo = -pillarWidth; xo <= pillarWidth; xo++) {
            for (int32_t zo = -pillarWidth; zo <= pillarWidth; zo++) {
                const int32_t cx = originX + xo;
                int32_t cy = originY - 1;
                const int32_t cz = originZ + zo;
                int32_t runLength = 50;
                if (std::abs(xo) == 1 && std::abs(zo) == 1)
                    runLength = mRandom.nextInt(5);

                while (cy > 50) {
                    const std::string state = manager.getBlockAt(cx, cy, cz).mName;
                    if (state != "minecraft:air" && !canReplace(state) && state != "minecraft:packed_ice")
                        break;

                    manager.setBlockStateAt(cx, cy, cz, spikeState());
                    cy--;
                    if (--runLength <= 0) {
                        cy -= mRandom.nextInt(5) + 1;
                        runLength = mRandom.nextInt(5);
                    }
                }
            }
        }

        const int32_t snowRadius = std::max(2, width + 2);
        const int32_t topY = originY + height + 1;
        const int32_t bottomY = std::max(LevelChunk::MIN_Y + 1, originY - 2);

        for (int32_t x = originX - snowRadius; x <= originX + snowRadius; x++) {
            for (int32_t z = originZ - snowRadius; z <= originZ + snowRadius; z++) {
                for (int32_t y = topY; y >= bottomY; y--) {
                    const std::string here = manager.getBlockAt(x, y, z).mName;
                    if (here == "minecraft:packed_ice") {
                        const std::string above = manager.getBlockAt(x, y + 1, z).mName;
                        if (above == "minecraft:air")
                            manager.setBlockStateAt(x, y + 1, z, snowLayerState());

                        break;
                    }

                    if (here != "minecraft:air" && here != "minecraft:snow_layer")
                        break;
                }
            }
        }
    }

    queueObject(manager);
}
