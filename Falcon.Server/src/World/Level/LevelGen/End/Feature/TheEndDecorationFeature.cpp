#include "World/Level/LevelGen/End/Feature/TheEndDecorationFeature.h"

#include "World/Level/Block/ChorusPlantBlock.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
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
    const double TWO_PI = 6.283185307179586;

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &endStoneState() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    const BlockState &obsidianState() {
        static const BlockState state = VanillaBlocks::OBSIDIAN().toBlockState();
        return state;
    }

    const BlockState &ironBarsState() {
        static const BlockState state = VanillaBlocks::IRON_BARS().toBlockState();
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

    // the game reads its 65536 entry sine table
    float tableSine(float index) {
        return (float) std::sin((double) (uint16_t) (int32_t) index * TWO_PI / 65536.0);
    }

    int32_t nextInt(std::mt19937 &random, uint32_t bound) {
        return (int32_t) (random() % bound);
    }

}

TheEndDecorationFeature::TheEndDecorationFeature(int64_t seed, const TheEndSimplexNoise &islandNoise)
        : mSeed((uint32_t) seed), mIslandNoise(&islandNoise), mSpikes(_createSpikes((uint32_t) seed)) {
}

const char *TheEndDecorationFeature::name() const {
    return "the_end_decoration";
}

std::vector<TheEndDecorationFeature::Spike> TheEndDecorationFeature::_createSpikes(uint32_t seed) {
    std::mt19937 random(seed);

    int32_t sizes[10];
    for (int32_t i = 0; i < 10; i++)
        sizes[i] = i;

    for (int32_t i = 1; i < 10; i++)
        std::swap(sizes[i], sizes[nextInt(random, (uint32_t) i + 1)]);

    std::vector<Spike> spikes;
    spikes.reserve(10);

    for (int32_t i = 0; i < 10; i++) {
        const float angle = (float) i * 0.31415927f + -3.1415927f;
        const float index = (angle + angle) * 10430.378f;
        const int32_t size = sizes[i];
        spikes.push_back(Spike{(int32_t) (tableSine(index + 16384.0f) * 42.0f), (int32_t) (tableSine(index) * 42.0f),
                               size / 3 + 2, size * 3 + 76, size == 1 || size == 2});
    }

    return spikes;
}

int32_t TheEndDecorationFeature::_surfaceAt(int32_t x, int32_t z) const {
    const int32_t top = mRoot->getHeightAt(x, z);
    return top > 0 ? top + 1 : 0;
}

void TheEndDecorationFeature::_placeSpike(const Spike &spike) {
    const int32_t radius = spike.mRadius;

    for (int32_t x = -radius; x <= radius; x++) {
        for (int32_t y = 0; y <= spike.mHeight + 10; y++) {
            for (int32_t z = -radius; z <= radius; z++) {
                if (x * x + z * z <= radius * radius + 1 && y < spike.mHeight)
                    mRoot->setBlockStateAt(spike.mX + x, y, spike.mZ + z, obsidianState());
                else if (y >= 66)
                    mRoot->setBlockStateAt(spike.mX + x, y, spike.mZ + z, airState());
            }
        }
    }

    if (!spike.mGuarded)
        return;

    for (int32_t x = -2; x <= 2; x++) {
        for (int32_t z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 || std::abs(z) == 2) {
                for (int32_t y = 0; y < 3; y++)
                    mRoot->setBlockStateAt(spike.mX + x, spike.mHeight + y, spike.mZ + z, ironBarsState());
            }

            mRoot->setBlockStateAt(spike.mX + x, spike.mHeight + 3, spike.mZ + z, ironBarsState());
        }
    }
}

void TheEndDecorationFeature::_placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z) {
    float radius = (float) nextInt(random, 3) + 4.0f;

    for (int32_t offsetY = 0; radius > 0.5f; offsetY--) {
        const int32_t low = (int32_t) std::floor(-radius);
        const int32_t high = (int32_t) std::ceil(radius);
        const float limit = (radius + 1.0f) * (radius + 1.0f);

        for (int32_t offsetX = low; offsetX <= high; offsetX++) {
            for (int32_t offsetZ = low; offsetZ <= high; offsetZ++) {
                if (limit >= (float) (offsetX * offsetX + offsetZ * offsetZ))
                    mRoot->setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, endStoneState());
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

void TheEndDecorationFeature::_placeGateway(int32_t x, int32_t y, int32_t z) {
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetY = -2; offsetY <= 2; offsetY++) {
            for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                const bool center = offsetX == 0 && offsetZ == 0;
                const bool frame = std::abs(offsetY) == 2 ? center
                                                          : offsetY != 0 && (offsetX == 0 || offsetZ == 0);
                const BlockState &state = offsetY == 0 && center ? endGatewayState()
                                                                 : frame ? infiniburnBedrockState() : airState();
                mRoot->setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, state);
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
    for (const Spike &spike: mSpikes) {
        if (((spike.mX - spike.mRadius) & ~15) == baseX && ((spike.mZ - spike.mRadius) & ~15) == baseZ)
            _placeSpike(spike);
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
        _placeGateway(x, (int32_t) (random() % 7u) + surface + 3, z);
}
