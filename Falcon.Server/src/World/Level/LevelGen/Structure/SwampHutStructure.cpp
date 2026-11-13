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
#include "World/Level/LevelGen/Structure/ScatteredPiece.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <random>

namespace {
    const int32_t SIZE_X = 7;
    const int32_t SIZE_Z = 9;

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
            total += ScatteredPiece::aboveTopSolid(chunk, x, z);
    }
    const int32_t floorY = total / (SIZE_X * SIZE_Z) + 1;

    const BlockState planks = VanillaBlocks::SPRUCE_PLANKS().toBlockState();
    const BlockState log = VanillaBlocks::OAK_LOG().toBlockState();
    const BlockState fence = VanillaBlocks::OAK_FENCE().toBlockState();
    const BlockState air = VanillaBlocks::AIR().toBlockState();
    const BlockState stairs = VanillaBlocks::SPRUCE_STAIRS().toBlockState();

    ScatteredPiece hut(level, chunk, cornerX, floorY, cornerZ);
    hut.generateBox(0, 2, 0, 6, 3, 8, air, air, true);
    hut.generateBox(1, 1, 1, 5, 1, 7, planks, planks, false);
    hut.generateBox(1, 5, 2, 5, 5, 7, planks, planks, false);
    hut.generateBox(2, 1, 0, 4, 1, 0, planks, planks, false);
    hut.generateBox(2, 2, 2, 3, 4, 2, planks, planks, false);
    hut.generateBox(1, 2, 3, 1, 4, 6, planks, planks, false);
    hut.generateBox(5, 2, 3, 5, 4, 6, planks, planks, false);
    hut.generateBox(2, 2, 7, 4, 4, 7, planks, planks, false);
    hut.generateBox(1, 0, 2, 1, 4, 2, log, log, false);
    hut.generateBox(5, 0, 2, 5, 4, 2, log, log, false);
    hut.generateBox(1, 0, 7, 1, 4, 7, log, log, false);
    hut.generateBox(5, 0, 7, 5, 4, 7, log, log, false);
    hut.placeBlock(fence, 2, 3, 2);
    hut.placeBlock(fence, 3, 3, 7);
    hut.placeBlock(air, 1, 3, 4);
    hut.placeBlock(air, 5, 3, 4);
    hut.placeBlock(air, 5, 3, 5);
    hut.placeBlock(VanillaBlocks::FLOWER_POT().toBlockState(), 1, 3, 5);
    hut.placeBlock(planks, 4, 4, 2);
    hut.placeBlock(VanillaBlocks::CRAFTING_TABLE().toBlockState(), 3, 2, 6);

    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ());
    const int32_t fillLevel = 2 * FeatureRandom::nextInt(random, 3) + 2;
    hut.placeBlock(DecorationSupport::withState(VanillaBlocks::CAULDRON().toBlockState(), "fill_level", fillLevel), 4,
                   2, 6);
    const int16_t potion = cauldronPotion(FeatureRandom::nextInt(random, 100));

    hut.placeBlock(fence, 1, 2, 1);
    hut.placeBlock(fence, 5, 2, 1);

    hut.generateBox(0, 5, 1, 6, 5, 1, ScatteredPiece::stairs(stairs, 3), ScatteredPiece::stairs(stairs, 3), false);
    hut.generateBox(0, 5, 2, 0, 5, 7, ScatteredPiece::stairs(stairs, 0), ScatteredPiece::stairs(stairs, 0), false);
    hut.generateBox(6, 5, 2, 6, 5, 7, ScatteredPiece::stairs(stairs, 1), ScatteredPiece::stairs(stairs, 1), false);
    hut.generateBox(0, 5, 8, 6, 5, 8, ScatteredPiece::stairs(stairs, 2), ScatteredPiece::stairs(stairs, 2), false);

    hut.fillColumnDown(log, 1, -1, 2);
    hut.fillColumnDown(log, 5, -1, 2);
    hut.fillColumnDown(log, 1, -1, 7);
    hut.fillColumnDown(log, 5, -1, 7);

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
