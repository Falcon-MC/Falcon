#include "World/Level/LevelGen/End/Feature/TheEndDecorationFeature.h"

#include "World/Actor/Misc/EndCrystalActor.h"
#include "World/Level/Block/ChorusPlantBlock.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"

#include <cmath>
#include <cstdlib>
#include <utility>

namespace {

    const int32_t CHORUS_MAX_SPREAD = 8;

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &endStoneState() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    const BlockState &endGatewayState() {
        static const BlockState state = VanillaBlocks::END_GATEWAY().toBlockState();
        return state;
    }

    const BlockState &chorusPlantState() {
        static const BlockState state = VanillaBlocks::CHORUS_PLANT().toBlockState();
        return state;
    }

    const BlockState &agedChorusFlowerState() {
        static const BlockState state = []() {
            BlockState flower = VanillaBlocks::CHORUS_FLOWER().toBlockState();
            flower.mStates.putInt("age", 5);
            return BlockState(flower.mName, flower.mStates);
        }();
        return state;
    }

    const BlockState &infiniburnBedrockState() {
        static const BlockState state = []() {
            BlockState bedrock = VanillaBlocks::BEDROCK().toBlockState();
            bedrock.mStates.putByte("infiniburn_bit", 1);
            return BlockState(bedrock.mName, bedrock.mStates);
        }();
        return state;
    }

    bool isAir(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    int32_t nextInt(std::mt19937 &random, uint32_t bound) {
        return (int32_t) (random() % bound);
    }

}

TheEndDecorationFeature::TheEndDecorationFeature(int64_t seed, const TheEndSimplexNoise &islandNoise)
        : mSeed((uint32_t) seed), mIslandNoise(&islandNoise), mSpikes(EndSpikes::create((uint32_t) seed)),
          mSpikeRandom((uint32_t) seed) {
}

const char *TheEndDecorationFeature::name() const {
    return "the_end_decoration";
}

int32_t TheEndDecorationFeature::_surfaceAt(int32_t x, int32_t z) const {
    const int32_t top = mRoot->getHeightAt(x, z);
    return top > 0 ? top + 1 : 0;
}

void TheEndDecorationFeature::_placeSpike(ChunkGenerateContext &context, const EndSpike &spike) {
    EndSpikes::place(spike, true, [this](int32_t x, int32_t y, int32_t z, const BlockState &state) {
        mRoot->setBlockStateAt(x, y, z, state);
    });

    // sub_14D1EE710 sets each spike's crystal on bedrock once the spike stands
    const Vector3i top = spike.getTop();
    mRoot->setBlockStateAt(top.x, top.y, top.z, EndSpikes::getCrystalBaseState());

    const Vector3f position = spike.getCrystalPosition();
    Tag location = Tag::ofList(Tag::Type::Float);
    location.addToList(Tag::ofFloat(position.x));
    location.addToList(Tag::ofFloat(position.y));
    location.addToList(Tag::ofFloat(position.z));

    Tag rotation = Tag::ofList(Tag::Type::Float);
    rotation.addToList(Tag::ofFloat((float) (mSpikeRandom() % 360u)));
    rotation.addToList(Tag::ofFloat(0.0f));

    Tag crystal = Tag::ofCompound();
    crystal.putString("identifier", EndCrystalActor::IDENTIFIER);
    crystal.put("Pos", location);
    crystal.put("Rotation", rotation);
    crystal.putBool(EndCrystalActor::SHOW_BOTTOM_TAG, true);
    context.getLevel().addGeneratedEntity(std::move(crystal));
}

void TheEndDecorationFeature::_placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z) {
    placeIsland(random, x, y, z, [this](int32_t blockX, int32_t blockY, int32_t blockZ, const BlockState &state) {
        mRoot->setBlockStateAt(blockX, blockY, blockZ, state);
    });
}

void TheEndDecorationFeature::placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z,
                                          const EndSpikes::BlockSink &sink) {
    float radius = (float) nextInt(random, 3) + 4.0f;

    for (int32_t offsetY = 0; radius > 0.5f; offsetY--) {
        const int32_t low = (int32_t) std::floor(-radius);
        const int32_t high = (int32_t) std::ceil(radius);
        const float limit = (radius + 1.0f) * (radius + 1.0f);

        for (int32_t offsetX = low; offsetX <= high; offsetX++) {
            for (int32_t offsetZ = low; offsetZ <= high; offsetZ++) {
                if (limit >= (float) (offsetX * offsetX + offsetZ * offsetZ))
                    sink(x + offsetX, y + offsetY, z + offsetZ, endStoneState());
            }
        }

        radius -= (float) nextInt(random, 2) + 0.5f;
    }
}

bool TheEndDecorationFeature::_isClearOfChorus(int32_t x, int32_t y, int32_t z, int32_t exceptX,
                                               int32_t exceptZ) const {
    static const int32_t SIDES[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    for (const int32_t *side: SIDES) {
        if (side[0] == exceptX && side[1] == exceptZ)
            continue;

        if (!isAir(mRoot->getBlockAt(x + side[0], y, z + side[1])))
            return false;
    }

    return true;
}

void TheEndDecorationFeature::_growChorus(std::mt19937 &random, const Vector3i &origin, const Vector3i &position,
                                          int32_t age, std::vector<Vector3i> &plants) {
    static const int32_t SIDES[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    const int32_t height = (age == 0 ? 1 : 0) + nextInt(random, 4) + 1;

    for (int32_t i = 1; i <= height; i++) {
        if (!_isClearOfChorus(position.x, position.y + i, position.z, 0, 0))
            return;

        mRoot->setBlockStateAt(position.x, position.y + i, position.z, chorusPlantState());
        plants.emplace_back(position.x, position.y + i, position.z);
    }

    bool branched = false;
    if (age <= 3) {
        const int32_t attempts = nextInt(random, 4) + (age == 0 ? 1 : 0);

        for (int32_t i = 0; i < attempts; i++) {
            const int32_t *side = SIDES[nextInt(random, 4)];
            const Vector3i branch(position.x + side[0], position.y + height, position.z + side[1]);

            if (std::abs(origin.x - branch.x) >= CHORUS_MAX_SPREAD || std::abs(origin.z - branch.z) >= CHORUS_MAX_SPREAD
                || !isAir(mRoot->getBlockAt(branch.x, branch.y, branch.z))
                || !isAir(mRoot->getBlockAt(branch.x, branch.y - 1, branch.z))
                || !_isClearOfChorus(branch.x, branch.y, branch.z, -side[0], -side[1]))
                continue;

            branched = true;
            mRoot->setBlockStateAt(branch.x, branch.y, branch.z, chorusPlantState());
            plants.push_back(branch);
            _growChorus(random, origin, branch, age + 1, plants);
        }
    }

    if (!branched)
        mRoot->setBlockStateAt(position.x, position.y + height, position.z, agedChorusFlowerState());
}

void TheEndDecorationFeature::_placeChorus(std::mt19937 &random, int32_t x, int32_t y, int32_t z) {
    const Vector3i origin(x, y, z);
    std::vector<Vector3i> plants{origin};

    mRoot->setBlockStateAt(x, y, z, chorusPlantState());
    _growChorus(random, origin, origin, 0, plants);

    for (const Vector3i &plant: plants) {
        const BlockState state = mRoot->getBlockAt(plant.x, plant.y, plant.z);
        // measured against BDS: a generated plant does not connect down to the end stone it grows on
        mRoot->setBlockStateAt(plant.x, plant.y, plant.z, ChorusPlantBlock::connectedState(state, [&](int face) {
            const Vector3i other = PlacementOrientation::relativePosition(plant, face);
            const BlockState &neighbour = mRoot->getBlockAt(other.x, other.y, other.z);
            const bool standsOn = face == PlacementOrientation::FACE_DOWN && neighbour.mName == endStoneState().mName;
            return standsOn ? &airState() : &neighbour;
        }));
    }
}

void TheEndDecorationFeature::placeGateway(int32_t x, int32_t y, int32_t z, const EndSpikes::BlockSink &sink) {
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetY = -2; offsetY <= 2; offsetY++) {
            for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                const bool center = offsetX == 0 && offsetZ == 0;
                const bool frame = std::abs(offsetY) == 2 ? center
                                                          : offsetY != 0 && (offsetX == 0 || offsetZ == 0);
                const BlockState &state = offsetY == 0 && center ? endGatewayState()
                                                                 : frame ? infiniburnBedrockState() : airState();
                sink(x + offsetX, y + offsetY, z + offsetZ, state);
            }
        }
    }
}

void TheEndDecorationFeature::apply(ChunkGenerateContext &context) {
    if (mRoot == nullptr)
        return;

    const LevelChunk &chunk = context.getChunk();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int32_t baseX = chunkX << 4;
    const int32_t baseZ = chunkZ << 4;

    std::mt19937 seeding(mSeed);
    const uint32_t multiplierX = (seeding() >> 1) | 1u;
    const uint32_t multiplierZ = (seeding() >> 1) | 1u;
    const uint32_t decorationSeed = (multiplierZ * (uint32_t) chunkZ + multiplierX * (uint32_t) chunkX) ^ mSeed;
    std::mt19937 random(decorationSeed);

    // sub_14CDABCD0 runs the biome's rules on this random before the end features, and they leave it reseeded
    OverworldRulePlacer::drawBiomeRules(*mRoot, chunk, TheEndGeneratorConstants::BIOME_ID, decorationSeed, random);

    // a spike belongs to the chunk of its lowest corner
    for (const EndSpike &spike: mSpikes) {
        if (((spike.mX - spike.mRadius) & ~15) == baseX && ((spike.mZ - spike.mRadius) & ~15) == baseZ)
            _placeSpike(context, spike);
    }

    if ((int64_t) chunkX * chunkX + (int64_t) chunkZ * chunkZ <= 4096LL)
        return;

    const float height = TheEndGenerator::getIslandHeight(chunkX, chunkZ, 1, 1, *mIslandNoise);

    if (height < -20.0f && random() % 14u == 0) {
        const int32_t x = baseX + 8 + (int32_t) (random() & 15u);
        const int32_t y = 55 + (int32_t) (random() & 15u);
        const int32_t z = baseZ + 8 + (int32_t) (random() & 15u);
        _placeIsland(random, x, y, z);

        if ((random() & 3u) == 0)
            _placeIsland(random, x, y, z);
    }

    if (height <= 40.0f)
        return;

    const uint32_t attempts = random() % 5u;
    uint32_t draw = random();

    for (uint32_t attempt = 0; attempt < attempts; attempt++) {
        const int32_t z = (baseZ | 8) + (int32_t) (random() & 15u);
        const int32_t x = (baseX | 8) + (int32_t) (draw & 15u);
        const int32_t y = _surfaceAt(x, z);
        if (y > 0 && isAir(mRoot->getBlockAt(x, y, z)) && mRoot->getBlockAt(x, y - 1, z).mName == "minecraft:end_stone")
            _placeChorus(random, x, y, z);

        draw = random();
    }

    if (draw % 700u != 0)
        return;

    const int32_t x = (baseX | 8) + (int32_t) (random() & 15u);
    const int32_t z = (baseZ | 8) + (int32_t) (random() & 15u);
    const int32_t surface = _surfaceAt(x, z);
    if (surface > 0)
        placeGateway(x, (int32_t) (random() % 7u) + surface + 3, z,
                     [this](int32_t blockX, int32_t blockY, int32_t blockZ, const BlockState &state) {
                         mRoot->setBlockStateAt(blockX, blockY, blockZ, state);
                     });
}
