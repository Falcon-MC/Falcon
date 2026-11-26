#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/SetBlockCommand.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const int32_t X = 24;
    const int32_t Z = 24;
    const int32_t FLOOR = 199;
    const int32_t TOP = 204;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void place(ServerNetworkHandler &owner, Level &level, int32_t y, const std::string &name) {
        BlockState state;
        require(SetBlockCommand::resolveBlock(name, state), "unknown block " + name);
        require(SetBlockCommand::placeBlock(owner, level, Vector3i(X, y, Z), state, "replace"),
                "setblock refused for " + name);
    }

    // "column", "pull" for a dragging column, or the block name
    std::string cell(Level &level, int32_t y) {
        const BlockState &state = *level.peekBlockPtr(X, y, Z);
        if (state.mName != "minecraft:bubble_column")
            return state.mName;
        return state.mStates.getBool("drag_down", false) ? "pull" : "column";
    }

    void withLevel(const std::function<void(ServerNetworkHandler &, Level &)> &body) {
        ServerNetworkHandler owner("Bubble columns", "", 1);
        Level level("bubble-columns", 4, 1);
        level.setOwner(&owner);
        std::vector<int64_t> columns;
        for (int32_t chunkX = 0; chunkX <= 2; ++chunkX) {
            for (int32_t chunkZ = 0; chunkZ <= 2; ++chunkZ) {
                level.getChunk(chunkX, chunkZ);
                columns.push_back(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
            }
        }
        level.setActiveColumns(columns);

        // a shaft of still water walled in glass, soul sand under it
        BlockState glass("minecraft:glass");
        Tag source = Tag::ofCompound();
        source.putInt("liquid_depth", 0);
        BlockState water("minecraft:water", source);
        for (int32_t y = FLOOR - 1; y <= TOP + 1; ++y) {
            for (int32_t x = X - 1; x <= X + 1; ++x) {
                for (int32_t z = Z - 1; z <= Z + 1; ++z) {
                    const bool inside = x == X && z == Z && y > FLOOR && y <= TOP;
                    level.setBlockState(x, y, z, y > TOP ? BlockState("minecraft:air") : inside ? water : glass);
                }
            }
        }
        place(owner, level, FLOOR, "minecraft:soul_sand");
        for (int tick = 0; tick < 40; ++tick)
            level.tick();
        for (int32_t y = FLOOR + 1; y <= TOP; ++y)
            require(cell(level, y) == "column", "soul sand grew " + cell(level, y) + " at " + std::to_string(y));
        body(owner, level);
    }

    // the column stays and turns to pull one block every six ticks, from the bottom up
    void magmaTurnsTheColumn() {
        withLevel([](ServerNetworkHandler &owner, Level &level) {
            place(owner, level, FLOOR, "minecraft:magma");
            for (int tick = 1; tick <= 40; ++tick) {
                level.tick();
                for (int32_t y = FLOOR + 1; y <= TOP; ++y) {
                    const int turned = 6 * (y - FLOOR);
                    const std::string expected = tick >= turned ? "pull" : "column";
                    require(cell(level, y) == expected, "tick " + std::to_string(tick) + ": " + cell(level, y)
                                                                + " at " + std::to_string(y) + " instead of "
                                                                + expected);
                }
            }
        });
    }

    // without soul sand, magma or a column below the column goes, six ticks after each loss
    void stoneEndsTheColumn() {
        withLevel([](ServerNetworkHandler &owner, Level &level) {
            place(owner, level, FLOOR, "minecraft:stone");
            for (int tick = 1; tick <= 40; ++tick) {
                level.tick();
                for (int32_t y = FLOOR + 1; y <= TOP; ++y) {
                    const int gone = 6 * (y - FLOOR);
                    const std::string expected = tick >= gone ? "minecraft:water" : "column";
                    require(cell(level, y) == expected, "tick " + std::to_string(tick) + ": " + cell(level, y)
                                                                + " at " + std::to_string(y) + " instead of "
                                                                + expected);
                }
            }
        });
    }
}

int main() {
    try {
        magmaTurnsTheColumn();
        stoneEndsTheColumn();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
