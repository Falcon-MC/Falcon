#include "World/Level/LevelGen/Structure/SwampHutStructure.h"

#include "World/Actor/Mob/Hostile/WitchActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Passive/CatActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/Actor/FlowerPotBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"

#include <random>

namespace {
    const int32_t SIZE_X = 7;
    const int32_t SIZE_Z = 9;

    // LevelChunk::getAboveTopSolidBlock (sub_1438650A0, 26.60) with water counted and leaves looked through
    int32_t aboveTopSolid(const LevelChunk &chunk, int32_t x, int32_t z) {
        for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
            const BlockState &state = chunk.getBlock(x, y, z);
            if (StructureMaterials::stopsTopSolidSearch(state, true, false))
                return y + 1;
        }
        return LevelChunk::MIN_Y;
    }

    // the piece's box of blocks in its own coordinates; the hut is never turned: they are the chunk's too
    class Hut {
    public:
        Hut(LevelChunk &chunk, int32_t floorY) : mChunk(chunk), mFloorY(floorY) {}

        void set(int32_t x, int32_t y, int32_t z, const BlockState &state) {
            const int32_t worldY = mFloorY + y;
            if (worldY >= LevelChunk::MIN_Y && worldY <= LevelChunk::MAX_Y)
                mChunk.setBlock(x, worldY, z, state);
        }

        void fill(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                  const BlockState &state) {
            for (int32_t y = minY; y <= maxY; y++) {
                for (int32_t x = minX; x <= maxX; x++) {
                    for (int32_t z = minZ; z <= maxZ; z++)
                        set(x, y, z, state);
                }
            }
        }

        // StructurePiece::generateBox with its last flag set: only what is not air already
        void clear(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ) {
            fill(minX, minY, minZ, maxX, maxY, maxZ, VanillaBlocks::AIR().toBlockState());
        }

        // StructurePiece::fillColumnDown (sub_147772AF0, 26.60): down through air, liquids and small plants, then
        // grass under the column turns to dirt
        void fillColumnDown(int32_t x, int32_t y, int32_t z, const BlockState &state) {
            int32_t worldY = mFloorY + y;
            while (worldY > LevelChunk::MIN_Y + 1) {
                const BlockState &current = mChunk.getBlock(x, worldY, z);
                if (!DecorationSupport::isAir(current) && !StructureMaterials::isLiquid(current)
                    && !StructureMaterials::isPlant(current))
                    break;

                mChunk.setBlock(x, worldY, z, state);
                worldY--;
            }
            if (mChunk.getBlock(x, worldY, z).mName == VanillaBlocks::GRASS_BLOCK().getIdentifier())
                mChunk.setBlock(x, worldY, z, VanillaBlocks::DIRT().toBlockState());
        }

    private:
        LevelChunk &mChunk;
        int32_t mFloorY;
    };

    // the potion ids the game draws for the hut's cauldron: water breathing, fire resistance, swiftness,
    // slowness, poison, weakness, healing
    int16_t cauldronPotion(int32_t roll) {
        if (roll < 10)
            return 19;
        if (roll < 15)
            return 12;
        if (roll < 30)
            return 14;
        if (roll < 40)
            return 17;
        if (roll < 65)
            return 25;
        if (roll <= 74)
            return 34;
        return 21;
    }

    Tag generatedMob(const char *identifier, float x, float y, float z) {
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat(x));
        location.addToList(Tag::ofFloat(y));
        location.addToList(Tag::ofFloat(z));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", identifier);
        mob.put("Pos", location);
        return mob;
    }
}

// SwampHutPiece::postProcess (sub_1473DADC0, 26.60) and postProcessMobsAt (sub_1473DC300)
void SwampHutStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *temple = StructureFeature::get("temple");
    if (temple == nullptr || !temple->isStartChunk(generator, chunk.getX(), chunk.getZ()))
        return;

    const int32_t cornerX = chunk.getX() * 16;
    const int32_t cornerZ = chunk.getZ() * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(chunk.getX() * 4, chunk.getZ() * 4);
    const int32_t biome = generator.pickBiome(cornerX + 8, surface, cornerZ + 8);
    if (biome != BiomeIds::SWAMPLAND && biome != BiomeIds::SWAMPLAND_MUTATED)
        return;

    // sub_1473CE840: the box sits one block over the mean ground of its columns
    int32_t total = 0;
    for (int32_t z = 0; z < SIZE_Z; z++) {
        for (int32_t x = 0; x < SIZE_X; x++)
            total += aboveTopSolid(chunk, x, z);
    }
    const int32_t floorY = total / (SIZE_X * SIZE_Z) + 1;

    const BlockState planks = VanillaBlocks::SPRUCE_PLANKS().toBlockState();
    const BlockState log = VanillaBlocks::OAK_LOG().toBlockState();
    const BlockState fence = VanillaBlocks::OAK_FENCE().toBlockState();
    const BlockState air = VanillaBlocks::AIR().toBlockState();
    const BlockState stairs = VanillaBlocks::SPRUCE_STAIRS().toBlockState();

    Hut hut(chunk, floorY);
    hut.clear(0, 2, 0, 6, 3, 8);
    hut.fill(1, 1, 1, 5, 1, 7, planks);
    hut.fill(1, 5, 2, 5, 5, 7, planks);
    hut.fill(2, 1, 0, 4, 1, 0, planks);
    hut.fill(2, 2, 2, 3, 4, 2, planks);
    hut.fill(1, 2, 3, 1, 4, 6, planks);
    hut.fill(5, 2, 3, 5, 4, 6, planks);
    hut.fill(2, 2, 7, 4, 4, 7, planks);
    hut.fill(1, 0, 2, 1, 4, 2, log);
    hut.fill(5, 0, 2, 5, 4, 2, log);
    hut.fill(1, 0, 7, 1, 4, 7, log);
    hut.fill(5, 0, 7, 5, 4, 7, log);
    hut.set(2, 3, 2, fence);
    hut.set(3, 3, 7, fence);
    hut.set(1, 3, 4, air);
    hut.set(5, 3, 4, air);
    hut.set(5, 3, 5, air);
    hut.set(1, 3, 5, VanillaBlocks::FLOWER_POT().toBlockState());
    hut.set(4, 4, 2, planks);
    hut.set(3, 2, 6, VanillaBlocks::CRAFTING_TABLE().toBlockState());

    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ());
    const int32_t fillLevel = 2 * FeatureRandom::nextInt(random, 3) + 2;
    hut.set(4, 2, 6, DecorationSupport::withState(VanillaBlocks::CAULDRON().toBlockState(), "fill_level", fillLevel));
    const int16_t potion = cauldronPotion(FeatureRandom::nextInt(random, 100));

    hut.set(1, 2, 1, fence);
    hut.set(5, 2, 1, fence);

    // StructurePiece::getOrientationData (sub_147772040, 26.60): stairs take the weirdo_direction branch
    // (sub_147771BF0), an unturned piece swaps south and north
    hut.fill(0, 5, 1, 6, 5, 1, DecorationSupport::withState(stairs, "weirdo_direction", 2));
    hut.fill(0, 5, 2, 0, 5, 7, DecorationSupport::withState(stairs, "weirdo_direction", 0));
    hut.fill(6, 5, 2, 6, 5, 7, DecorationSupport::withState(stairs, "weirdo_direction", 1));
    hut.fill(0, 5, 8, 6, 5, 8, DecorationSupport::withState(stairs, "weirdo_direction", 3));

    hut.fillColumnDown(1, -1, 2, log);
    hut.fillColumnDown(5, -1, 2, log);
    hut.fillColumnDown(1, -1, 7, log);
    hut.fillColumnDown(5, -1, 7, log);

    BlockActorStore &actors = level.getBlockActors();
    actors.getOrCreate<FlowerPotBlockActor>(Vector3i(cornerX + 1, floorY + 3, cornerZ + 5))
            .setPlant(VanillaBlocks::RED_MUSHROOM().toBlockState());
    actors.getOrCreate<CauldronBlockActor>(Vector3i(cornerX + 4, floorY + 2, cornerZ + 6)).setPotion(potion);

    const float mobX = (float) (cornerX + 2) + 0.5f;
    const float mobZ = (float) (cornerZ + 5) + 0.5f;
    level.addGeneratedEntity(generatedMob(WitchActor::IDENTIFIER, mobX, (float) (floorY + 2), mobZ));
    Tag cat = generatedMob(CatActor::IDENTIFIER, mobX, (float) (floorY + 2), mobZ);
    cat.putString(MobActor::TAG_SPAWN_EVENT, "minecraft:spawn_midnight_cat");
    level.addGeneratedEntity(cat);
}
