#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcePatchFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &patchState() {
        static const BlockState state = VanillaBlocks::PACKED_ICE().toBlockState();
        return state;
    }

    bool isReplaceable(const std::string &id) {
        return id == "minecraft:snow"
               || id == "minecraft:snow_layer"
               || id == "minecraft:ice"
               || id == "minecraft:packed_ice"
               || id == "minecraft:dirt"
               || id == "minecraft:grass_block";
    }

}

const char *IcePatchFeature::name() const {
    return "minecraft:ice_patch_feature";
}

int32_t IcePatchFeature::getBase() const {
    return 1;
}

int32_t IcePatchFeature::getRandom() const {
    return 2;
}

void IcePatchFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    const int32_t localX = 3 + random.nextInt(10);
    const int32_t localZ = 3 + random.nextInt(10);
    const int32_t sourceX = (chunk.getX() << 4) + localX;
    const int32_t sourceZ = (chunk.getZ() << 4) + localZ;
    int32_t sourceY = chunk.getHeight(localX, localZ);

    if (DecorationSupport::getBiomeAt(level, sourceX, sourceY, sourceZ) != BiomeIds::ICE_PLAINS_SPIKES)
        return;

    BlockManager manager(level);
    while (sourceY > LevelChunk::MIN_Y + 1 && DecorationSupport::isAir(manager.getBlockAt(sourceX, sourceY, sourceZ)))
        sourceY--;

    const int32_t radius = 2 + random.nextInt(3);
    for (int32_t x = sourceX - radius; x <= sourceX + radius; x++) {
        for (int32_t z = sourceZ - radius; z <= sourceZ + radius; z++) {
            const int32_t dx = x - sourceX;
            const int32_t dz = z - sourceZ;
            if (dx * dx + dz * dz > radius * radius)
                continue;

            for (int32_t y = sourceY + 1; y >= sourceY - 2; y--) {
                const std::string id = manager.getBlockAt(x, y, z).mName;
                if (isReplaceable(id))
                    manager.setBlockStateAt(x, y, z, patchState());
                else if (id != "minecraft:air")
                    break;
            }
        }
    }

    queueObject(manager);
}
