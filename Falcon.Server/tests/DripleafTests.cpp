#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BigDripleafBlock.h"
#include "World/Level/Block/SmallDripleafBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>

namespace {
    using Tilt = BigDripleafBlock::Tilt;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState dripleaf(bool head, const char *direction = "east") {
        Tag states = VanillaBlocks::BIG_DRIPLEAF().toBlockState().mStates;
        states.putString("minecraft:cardinal_direction", direction);
        return BigDripleafBlock::withParts(BlockState(BigDripleafBlock::IDENTIFIER, states), head, Tilt::None);
    }

    BlockState at(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    void clearColumn(Level &level, int32_t x, int32_t z) {
        for (int32_t y = 95; y < 110; ++y)
            level.setBlock(Vector3i(x, y, z), BlockState("minecraft:air"), false);
    }

    void checkTilt(ServerNetworkHandler &server, Level &level, const BigDripleafBlock &block) {
        const Vector3i soil(2, 99, 2);
        const Vector3i leaf(2, 100, 2);
        clearColumn(level, leaf.x, leaf.z);
        level.setBlock(soil, BlockState("minecraft:clay"), false);
        level.setBlock(leaf, dripleaf(true), false);
        require(block.canSurvive(level, leaf, at(level, leaf)), "a leaf on clay did not survive");

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4003).first->second;
        player.moveToLevel(level);

        player.setPosition(Vector3f(2.5f, 100.5f, 2.5f));
        block.onStepOn(server, player, leaf, at(level, leaf));
        require(BigDripleafBlock::getTilt(at(level, leaf)) == Tilt::None, "a foot inside the leaf tipped it");

        player.setPosition(Vector3f(2.5f, 100.9375f, 2.5f));
        block.onStepOn(server, player, leaf, at(level, leaf));
        require(BigDripleafBlock::getTilt(at(level, leaf)) == Tilt::Unstable && level.isUpdateScheduled(leaf),
                "standing on the leaf did not make it unstable");

        const Tilt order[] = {Tilt::Partial, Tilt::Full, Tilt::None};
        for (const Tilt expected: order) {
            block.onScheduledUpdate(server, level, leaf, at(level, leaf));
            require(BigDripleafBlock::getTilt(at(level, leaf)) == expected, "the leaf skipped a tilt");
        }
        require(!level.isUpdateScheduled(leaf), "a flat leaf kept a pending tick");

        block.onStepOn(server, player, leaf, at(level, leaf));
        level.setBlock(Vector3i(3, 100, 2), BlockState("minecraft:redstone_block"), false);
        block.onScheduledUpdate(server, level, leaf, at(level, leaf));
        require(BigDripleafBlock::getTilt(at(level, leaf)) == Tilt::None, "power did not hold the leaf flat");
        level.setBlock(Vector3i(3, 100, 2), BlockState("minecraft:air"), false);
        player.setPosition(Vector3f(100.5f, 100.0f, 100.5f));
    }

    void checkShapes(const BigDripleafBlock &block) {
        AxisAlignedBB shape(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        require(block.getCollisionShape(dripleaf(true), nullptr, shape) && shape.mMinY == 0.6875f
                        && shape.mMaxY == 0.9375f,
                "wrong flat leaf shape");
        require(block.getCollisionShape(BigDripleafBlock::withParts(dripleaf(true), true, Tilt::Partial), nullptr,
                                        shape)
                        && shape.mMaxY == 0.8125f,
                "wrong partial leaf shape");
        require(block.getCollisionShape(BigDripleafBlock::withParts(dripleaf(true), true, Tilt::Full), nullptr, shape)
                        && shape.mMaxY <= shape.mMinY,
                "a fully tilted leaf still holds actors");
        require(block.getCollisionShape(dripleaf(false), nullptr, shape) && shape.mMaxY <= shape.mMinY,
                "a stem holds actors");
    }

    void checkColumn(ServerNetworkHandler &server, Level &level, const BigDripleafBlock &block) {
        const Vector3i base(5, 100, 5);
        clearColumn(level, base.x, base.z);
        level.setBlock(Vector3i(5, 99, 5), BlockState("minecraft:moss_block"), false);
        level.setBlock(base, dripleaf(true, "north"), false);
        const Vector3i second(5, 101, 5);
        level.setBlock(second, dripleaf(true, "south"), false);
        block.onPlacedInLevel(server, level, second, at(level, second));
        require(!BigDripleafBlock::isHead(at(level, base)) && BigDripleafBlock::isHead(at(level, second))
                        && at(level, second).mStates.getString("minecraft:cardinal_direction", "") == "north",
                "a leaf placed on a leaf did not take its direction and make it a stem");

        require(block.onFertilized(server, level, base, at(level, base)), "bone meal did not grow the column");
        const Vector3i top(5, 102, 5);
        require(BigDripleafBlock::isHead(at(level, top)) && !BigDripleafBlock::isHead(at(level, second))
                        && at(level, top).mStates.getString("minecraft:cardinal_direction", "") == "north",
                "the column did not grow by one leaf");

        level.setBlock(Vector3i(5, 103, 5), BlockState("minecraft:stone"), false);
        require(!block.onFertilized(server, level, base, at(level, base)), "a column grew into stone");

        level.setBlock(Vector3i(5, 103, 5), BlockState("minecraft:water"), false);
        require(block.onFertilized(server, level, base, at(level, base)), "a column did not grow into water");
        require(level.getBlockStateAtLayer(5, 103, 5, 1).mName == "minecraft:water",
                "the leaf grown into water lost the water");

        level.setBlock(Vector3i(5, 104, 5), BlockState("minecraft:air"), false);
        level.setBlock(top, BlockState("minecraft:air"), false);
        require(!block.canSurvive(level, second, at(level, second)), "a stem without a leaf above survived");

        level.setBlock(Vector3i(5, 99, 5), BlockState("minecraft:stone"), false);
        require(!block.canSurvive(level, base, BigDripleafBlock::withParts(at(level, base), true, Tilt::None)),
                "a leaf on stone survived");
    }

    void checkSmall(ServerNetworkHandler &server, Level &level, const SmallDripleafBlock &block) {
        for (int32_t attempt = 0; attempt < 20; ++attempt) {
            const Vector3i lower(8, 100, 8);
            const Vector3i upper(8, 101, 8);
            clearColumn(level, lower.x, lower.z);
            level.setBlock(Vector3i(8, 99, 8), BlockState("minecraft:clay"), false);
            Tag states = VanillaBlocks::SMALL_DRIPLEAF_BLOCK().toBlockState().mStates;
            states.putString("minecraft:cardinal_direction", "west");
            states.putByte("upper_block_bit", 0);
            level.setBlock(lower, BlockState("minecraft:small_dripleaf_block", states), false);
            states.putByte("upper_block_bit", 1);
            level.setBlock(upper, BlockState("minecraft:small_dripleaf_block", states), false);

            require(block.onFertilized(server, level, upper, at(level, upper)), "bone meal on the upper half failed");

            int32_t height = 0;
            while (at(level, Vector3i(8, 100 + height, 8)).mName == BigDripleafBlock::IDENTIFIER)
                ++height;
            require(height >= 2 && height <= 5, "the big dripleaf grew outside 2 to 5 blocks");
            for (int32_t y = 0; y < height; ++y) {
                const BlockState part = at(level, Vector3i(8, 100 + y, 8));
                require(BigDripleafBlock::isHead(part) == (y == height - 1)
                                && part.mStates.getString("minecraft:cardinal_direction", "") == "west",
                        "the grown column has the wrong parts or direction");
            }
        }

        std::vector<BlockDrop> drops;
        block.getDrops(at(level, Vector3i(8, 99, 8)), ItemStack::air(), 0, drops);
        require(drops.empty(), "a small dripleaf dropped without shears");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Dripleaf", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const auto *big = VanillaBlocks::getAs<BigDripleafBlock>(BigDripleafBlock::IDENTIFIER);
        const auto *small = VanillaBlocks::getAs<SmallDripleafBlock>("minecraft:small_dripleaf_block");
        require(big != nullptr && small != nullptr, "the dripleaf blocks were not registered");
        checkShapes(*big);
        checkTilt(server, level, *big);
        checkColumn(server, level, *big);
        checkSmall(server, level, *small);
    } catch (const std::exception &error) {
        std::clog << "Dripleaf regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
