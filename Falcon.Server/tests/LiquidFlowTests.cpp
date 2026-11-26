#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
    const int32_t X = 24;
    const int32_t Y = 200;
    const int32_t Z = 24;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState liquid(const char *name, int depth) {
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", depth);
        return BlockState(name, states);
    }

    // a stone room around (X, Y, Z), nothing ticked: the current only reads the blocks
    void clear(Level &level) {
        for (int32_t x = X - 3; x <= X + 3; ++x)
            for (int32_t z = Z - 3; z <= Z + 3; ++z)
                for (int32_t y = Y - 2; y <= Y + 2; ++y)
                    level.setBlockState(x, y, z, BlockState(y < Y ? "minecraft:stone" : "minecraft:air"));
    }

    void expect(Level &level, float x, float y, float z, const std::string &name) {
        const Vector3f flow = level.getLiquidFlowVector(Vector3i(X, Y, Z));
        const bool same = std::fabs(flow.x - x) < 1e-5f && std::fabs(flow.y - y) < 1e-5f
                          && std::fabs(flow.z - z) < 1e-5f;
        require(same, name + ": (" + std::to_string(flow.x) + ", " + std::to_string(flow.y) + ", "
                              + std::to_string(flow.z) + ")");
    }

    void run() {
        Level level("liquid-flow", 4, 1);
        for (int32_t chunkX = 0; chunkX <= 2; ++chunkX)
            for (int32_t chunkZ = 0; chunkZ <= 2; ++chunkZ)
                level.getChunk(chunkX, chunkZ);

        // away from the source, the two sides cancel out
        clear(level);
        level.setBlockState(X - 1, Y, Z, liquid("minecraft:water", 0));
        level.setBlockState(X, Y, Z, liquid("minecraft:water", 1));
        level.setBlockState(X + 1, Y, Z, liquid("minecraft:water", 2));
        level.setBlockState(X, Y, Z - 1, liquid("minecraft:water", 2));
        level.setBlockState(X, Y, Z + 1, liquid("minecraft:water", 2));
        expect(level, 1.0f, 0.0f, 0.0f, "flat spread");

        // an open side over water counts 8 levels deeper
        clear(level);
        level.setBlockState(X, Y, Z, liquid("minecraft:water", 3));
        level.setBlockState(X, Y - 1, Z - 1, liquid("minecraft:water", 0));
        level.setBlockState(X, Y - 1, Z, BlockState("minecraft:stone"));
        expect(level, 0.0f, 0.0f, -1.0f, "edge pull");

        // water in stairs reads nothing through the stairs' closed back
        clear(level);
        Tag stairs = Tag::ofCompound();
        stairs.putInt("weirdo_direction", 0);
        stairs.putByte("upside_down_bit", 0);
        level.setBlockState(X, Y, Z, BlockState("minecraft:oak_stairs", stairs));
        level.setBlockStateAtLayer(X, Y, Z, 1, liquid("minecraft:water", 0));
        level.setBlockState(X + 1, Y, Z, liquid("minecraft:water", 3));
        level.setBlockState(X - 1, Y, Z, BlockState("minecraft:stone"));
        level.setBlockState(X, Y, Z - 1, BlockState("minecraft:stone"));
        level.setBlockState(X, Y, Z + 1, BlockState("minecraft:stone"));
        expect(level, 0.0f, 0.0f, 0.0f, "closed face");

        // falling water beside stone pulls straight down
        clear(level);
        level.setBlockState(X, Y - 1, Z, BlockState("minecraft:air"));
        level.setBlockState(X, Y, Z, liquid("minecraft:water", 8));
        level.setBlockState(X - 1, Y, Z, BlockState("minecraft:stone"));
        expect(level, 0.0f, -1.0f, 0.0f, "waterfall");
    }
}

int main() {
    try {
        run();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
