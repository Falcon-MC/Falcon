#include "World/Level/LevelGen/Nether/NetherCarvers.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <algorithm>
#include <cmath>

namespace {
    using GameRandom::nextFloat;
    using GameRandom::nextInt;
    using GameRandom::nextUnsigned;
    using GameRandom::toFloat;

    constexpr int32_t RANGE = 8;
    constexpr int32_t CAVE_BOUND = 10;
    constexpr int32_t SKIP_CARVE_CHANCE = 5;
    constexpr int32_t TOP_ROW = 119;
    constexpr float FLOOR_LEVEL = -0.7f;
    constexpr float PI = 3.1415927f;
    constexpr float HALF_PI = 1.5707964f;
    constexpr float TABLE_SCALE = 10430.378f;

    // the game reads sines from a 65536 entry table
    float tableSin(int32_t index) {
        return static_cast<float>(std::sin(static_cast<uint16_t>(index) * 6.283185307179586 / 65536.0));
    }

    float sine(float value) {
        return tableSin(static_cast<int32_t>(value * TABLE_SCALE));
    }

    float cosine(float value) {
        return tableSin(static_cast<int32_t>(value * TABLE_SCALE + 16384.0f));
    }

    int32_t floorInt(float value) {
        return static_cast<int32_t>(std::floor(value));
    }

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &netherrackState() {
        static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }
}

NetherCarvers::NetherCarvers(int64_t seed) : mSeed(static_cast<uint32_t>(seed)) {}

void NetherCarvers::carve(LevelChunk &chunk) const {
    // the feature rule seeds the chunk random with the level seed before placing
    std::mt19937 random(mSeed);
    const uint32_t multiplierX = static_cast<uint32_t>(nextInt(random)) | 1u;
    const uint32_t multiplierZ = static_cast<uint32_t>(nextInt(random)) | 1u;

    for (int32_t sourceX = chunk.getX() - RANGE; sourceX <= chunk.getX() + RANGE; sourceX++) {
        for (int32_t sourceZ = chunk.getZ() - RANGE; sourceZ <= chunk.getZ() + RANGE; sourceZ++) {
            random.seed(
                    mSeed
                    ^ (multiplierX * static_cast<uint32_t>(sourceX) + multiplierZ * static_cast<uint32_t>(sourceZ)));
            _addCaves(random, sourceX, sourceZ, chunk);
        }
    }
}

void NetherCarvers::_addCaves(std::mt19937 &random, int32_t sourceX, int32_t sourceZ, LevelChunk &chunk) {
    const int32_t count = nextInt(random, nextInt(random, nextInt(random, CAVE_BOUND) + 1) + 1);
    const int32_t skip = nextInt(random, SKIP_CARVE_CHANCE);
    if (count <= 0 || skip != 0)
        return;

    for (int32_t cave = 0; cave < count; cave++) {
        const int32_t z = (sourceZ << 4) + nextInt(random, 16);
        const int32_t y = nextInt(random, 128);
        const int32_t x = (sourceX << 4) + nextInt(random, 16);
        const Position start{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};

        int32_t tunnels = 1;
        if (nextInt(random, 4) == 0) {
            const float thickness = nextFloat(random) * 6.0f + 1.0f;
            _addTunnel(random, chunk, start, thickness, 0.0f, 0.0f, -1, -1, 0.5f);
            tunnels = nextInt(random, 4) + 1;
        }

        for (int32_t tunnel = 0; tunnel < tunnels; tunnel++) {
            const float yawRoll = nextFloat(random);
            const float yaw = yawRoll * PI + yawRoll * PI;
            const float pitchRoll = nextFloat(random) + -0.5f;
            const float pitch = (pitchRoll + pitchRoll) * 0.125f;
            const float thicknessRoll = nextFloat(random);
            const float width = nextFloat(random) + (thicknessRoll + thicknessRoll);
            _addTunnel(random, chunk, start, width + width, yaw, pitch, 0, 0, 0.5f);
        }
    }
}

void NetherCarvers::_addTunnel(std::mt19937 &parent, LevelChunk &chunk, Position position, float thickness, float yaw,
                               float pitch, int32_t start, int32_t length, float yScale) {
    // a start of -1 is a room, which walks from the middle of its length and stops once carved
    std::mt19937 random(static_cast<uint32_t>(nextInt(parent)));
    if (length <= 0)
        length = static_cast<int32_t>(112u - nextUnsigned(random) % 28u);

    const bool room = start == -1;
    const int32_t half = length >> 1;
    int32_t step = room ? half : start;
    const int32_t branch = (half != 0 ? static_cast<int32_t>(nextUnsigned(random) % static_cast<uint32_t>(half)) : 0)
                           + (length >> 2);
    const float steepness = nextUnsigned(random) % 6u != 0u ? 0.7f : 0.92f;

    const float centerX = static_cast<float>((chunk.getX() << 4) | 8);
    const float centerZ = static_cast<float>((chunk.getZ() << 4) | 8);
    const float reach = (thickness + 2.0f) + 16.0f;
    const float lengthF = static_cast<float>(length);

    float yawChange = 0.0f;
    float pitchChange = 0.0f;
    for (; step < length; step++) {
        const float wave = sine((static_cast<float>(step) * PI) / lengthF);
        const float radius = wave * (0.0f + thickness) + 1.5f;

        const float pitchCos = cosine(pitch);
        position.mX = cosine(yaw) * pitchCos + position.mX;
        position.mY = sine(pitch) + position.mY;
        position.mZ = pitchCos * sine(yaw) + position.mZ;

        const uint32_t rolls[6] = {random(), random(), random(), random(), random(), random()};
        const float pitchDrift = (toFloat(rolls[0]) - toFloat(rolls[1])) * toFloat(rolls[2]);
        const float yawDrift = (toFloat(rolls[3]) - toFloat(rolls[4])) * toFloat(rolls[5]);
        const float nextPitch = pitch * steepness + pitchChange * 0.1f;
        const float nextYaw = yaw + yawChange * 0.1f;
        yawChange = yawChange * 0.75f + yawDrift * 4.0f;
        pitchChange = pitchChange * 0.9f + pitchDrift * 2.0f;
        pitch = nextPitch;
        yaw = nextYaw;

        if (!room) {
            if (thickness > 1.0f && step == branch) {
                const float childPitch = pitch / 3.0f;
                const float firstThickness = toFloat(random()) * 0.5f + 0.5f;
                _addTunnel(random, chunk, position, firstThickness, yaw + -HALF_PI, childPitch, branch, length, 1.0f);
                const float secondThickness = toFloat(random()) * 0.5f + 0.5f;
                _addTunnel(random, chunk, position, secondThickness, yaw + HALF_PI, childPitch, branch, length, 1.0f);
                return;
            }

            if (random() % 4u == 0u)
                continue;
        }

        const float dx = position.mX - centerX;
        const float dz = position.mZ - centerZ;
        const float remaining = static_cast<float>(length - step);
        if ((dz * dz + dx * dx) - remaining * remaining > reach * reach)
            return;

        const float margin = radius + radius;
        if (centerX - 16.0f - margin > position.mX || centerZ - 16.0f - margin > position.mZ
            || position.mX > centerX + 16.0f + margin || position.mZ > margin + centerZ + 16.0f)
            continue;

        // a room that met lava keeps walking instead of stopping
        if (_carveStep(chunk, position, radius, yScale) && room)
            return;
    }
}

bool NetherCarvers::_carveStep(LevelChunk &chunk, const Position &position, float radius, float yScale) {
    const int32_t baseX = chunk.getX() << 4;
    const int32_t baseZ = chunk.getZ() << 4;
    const float verticalRadius = yScale * radius;

    const int32_t minX = std::max(floorInt(position.mX - radius) - 1 - baseX, 0);
    const int32_t maxX = std::min(floorInt(position.mX + radius) - baseX, 15);
    const int32_t minZ = std::max(floorInt(position.mZ - radius) - 1 - baseZ, 0);
    const int32_t maxZ = std::min(floorInt(position.mZ + radius) - baseZ, 15);
    const int32_t top = std::min(floorInt(position.mY + verticalRadius), TOP_ROW);
    const int32_t low = std::max(floorInt(position.mY - verticalRadius), 2);

    if (minX > maxX)
        return true;

    // inner columns only look at the two ends of the range
    const int32_t scanBottom = low - 2;
    for (int32_t x = minX; x <= maxX; x++) {
        const bool edgeX = x == minX || x == maxX;
        for (int32_t z = minZ; z <= maxZ; z++) {
            const bool edge = edgeX || z == minZ || z == maxZ;
            for (int32_t y = top + 2; y >= scanBottom; y--) {
                if (chunk.getBlock(x, y, z).getHash() == lavaState().getHash())
                    return false;

                if (!edge && y != scanBottom)
                    y = low - 1;
            }
        }
    }

    for (int32_t x = minX; x <= maxX; x++) {
        const float dx = ((static_cast<float>(x + baseX) + 0.5f) - position.mX) / radius;
        const float dx2 = dx * dx;
        for (int32_t z = minZ; z <= maxZ; z++) {
            const float dz = ((static_cast<float>(z + baseZ) + 0.5f) - position.mZ) / radius;
            const float dz2 = dz * dz;
            // the test runs one row below the block it removes
            for (int32_t row = top; row >= low - 1; row--) {
                const float dy = ((static_cast<float>(row) + 0.5f) - position.mY) / verticalRadius;
                if (!((dy * dy + dx2) + dz2 < 1.0f) || !(FLOOR_LEVEL < dy))
                    continue;

                // the game's other carvable blocks, dirt and grass, never reach a nether chunk at this stage
                if (chunk.getBlock(x, row + 1, z).getHash() == netherrackState().getHash())
                    chunk.setBlock(x, row + 1, z, airState());
            }
        }
    }

    return true;
}
