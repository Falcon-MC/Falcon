#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    // looked up at run time: block classes register during static initialisation
    const Block *water() {
        return VanillaBlocks::fromIdentifier("minecraft:water");
    }

    const Block *lava() {
        return VanillaBlocks::fromIdentifier("minecraft:lava");
    }

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::vector<Vector3i> runTick(BlockTickingQueue &queue, int64_t tick, const std::vector<int64_t> &columns) {
        std::vector<Vector3i> fired;
        queue.beginTick(tick);
        for (int64_t column: columns) {
            queue.tickColumn(tick, column, [&](const Vector3i &position, const Block *) {
                fired.push_back(position);
            }, [](int32_t, int32_t) {
                return true;
            });
        }
        queue.endTick();
        return fired;
    }

    void dueOrder() {
        BlockTickingQueue queue;
        const std::vector<int64_t> columns{BlockTickingQueue::column(0, 0)};
        queue.add(Vector3i(1, 64, 1), water(), 5);
        queue.add(Vector3i(2, 64, 2), water(), 3);
        require(runTick(queue, 3, columns).size() == 1, "only the tick due at 3 runs at 3");
        require(runTick(queue, 4, columns).empty(), "nothing is due at 4");
        const std::vector<Vector3i> fired = runTick(queue, 5, columns);
        require(fired.size() == 1 && fired[0].x == 1, "the tick due at 5 runs at 5");
    }

    void duplicates() {
        BlockTickingQueue queue;
        const Vector3i position(3, 64, 3);
        queue.add(position, water(), 5);
        queue.add(position, water(), 5);
        require(queue.isPending(position, water(), 5), "a queued tick is pending");
        require(!queue.isPending(position, lava(), 5), "another block's tick is not");
        require(queue.getScheduledCount() == 2, "the game keeps both copies");
        require(runTick(queue, 5, {BlockTickingQueue::column(0, 0)}).size() == 2, "both copies run");
        require(!queue.isPending(position, water(), 5), "nothing waits once both ran");
    }

    void chunkBudget() {
        BlockTickingQueue queue;
        for (int32_t index = 0; index < 150; ++index)
            queue.add(Vector3i(index % 16, 64 + index / 16, 0), water(), 1);
        const std::vector<int64_t> columns{BlockTickingQueue::column(0, 0)};
        require(runTick(queue, 1, columns).size() == BlockTickingQueue::MAX_TICKS_PER_CHUNK,
                "a chunk runs at most 100 ticks a tick");
        require(runTick(queue, 2, columns).size() == 50, "the rest runs on the next tick");
    }

    void laggingColumn() {
        BlockTickingQueue queue;
        const int64_t first = BlockTickingQueue::column(0, 0);
        const int64_t second = BlockTickingQueue::column(1, 0);
        queue.add(Vector3i(0, 64, 0), water(), 1);
        queue.beginTick(1);
        queue.tickColumn(1, first, [&](const Vector3i &, const Block *) {
            // a chunk later in the order has not had its turn: its clock still reads the last tick
            queue.add(Vector3i(16, 64, 0), water(), 5);
        }, [](int32_t, int32_t) {
            return true;
        });
        queue.tickColumn(1, second, [](const Vector3i &, const Block *) {
        }, [](int32_t, int32_t) {
            return true;
        });
        queue.endTick();
        require(runTick(queue, 5, {first, second}).size() == 1, "the waiting chunk counts from the last tick");
    }

    void randomClock() {
        BlockTickingQueue queue(true);
        const int64_t first = BlockTickingQueue::column(0, 0);
        queue.add(Vector3i(0, 64, 0), water(), 2);
        const auto runAt = [&](int64_t tick, int32_t speed) {
            size_t fired = 0;
            queue.beginTick(tick, speed);
            queue.tickColumn(tick, first, [&](const Vector3i &, const Block *) {
                ++fired;
            }, [](int32_t, int32_t) {
                return true;
            });
            queue.endTick();
            return fired;
        };
        require(runAt(1, 0) == 0 && runAt(2, 0) == 0, "a random tick speed of 0 stops the second queue");
        require(runAt(3, 1) == 0, "the second queue moves one step a tick at speed 1");
        require(runAt(4, 1) == 1, "the tick runs once its own clock reaches it");
    }

    void persistence() {
        Level level("tick-queue-test", 2, 1);
        LevelChunk &chunk = level.getChunk(0, 0);
        level.setActiveColumns({BlockTickingQueue::column(0, 0)});
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", 0);
        level.setBlockState(8, 200, 8, BlockState("minecraft:water", states));
        level.scheduleFluidTick(Vector3i(8, 200, 8), 5);
        const size_t queued = level.getScheduledFluidCount();
        require(queued > 0, "the water waits for its tick");

        LiquidPhysicsSystem &liquids = level.getLiquidPhysics();
        require(liquids.savePendingTicks(chunk), "a chunk with ticks gets a PendingTicks record");
        const std::string record = chunk.getPendingTicks();
        require(!record.empty(), "the record holds the ticks");
        liquids.dropPendingTicks(0, 0);
        require(level.getScheduledFluidCount() == 0, "an unloaded chunk keeps no ticks in memory");
        liquids.loadPendingTicks(chunk);
        require(level.getScheduledFluidCount() == queued, "the record brings every tick back");
        require(!liquids.savePendingTicks(chunk) && chunk.getPendingTicks() == record,
                "a reloaded queue saves the same record");
    }

    // old Falcon saved falling water as flowing_water at depth 0
    void migration() {
        Level level("tick-queue-migration", 2, 1);
        LevelChunk &chunk = level.getChunk(0, 0);
        Tag falling = Tag::ofCompound();
        falling.putInt("liquid_depth", 0);
        Tag spreading = Tag::ofCompound();
        spreading.putInt("liquid_depth", 3);
        chunk.setBlock(1, 200, 1, BlockState("minecraft:flowing_water", falling));
        chunk.setBlock(2, 200, 1, BlockState("minecraft:flowing_lava", spreading));
        chunk.setBlock(3, 200, 1, BlockState("minecraft:flowing_water", falling));
        level.getLiquidPhysics().dropPendingTicks(0, 0);

        chunk.setNeedsLiquidMigration(false);
        level.getLiquidPhysics().loadPendingTicks(chunk);
        require(chunk.getBlock(1, 200, 1).mName == "minecraft:flowing_water", "a chunk the game wrote stays as it is");

        chunk.setNeedsLiquidMigration(true);
        chunk.clearDirty();
        level.getLiquidPhysics().loadPendingTicks(chunk);
        const BlockState &water = chunk.getBlock(1, 200, 1);
        const BlockState &lava = chunk.getBlock(2, 200, 1);
        require(water.mName == "minecraft:water" && water.mStates.getInt("liquid_depth") == 8,
                "falling water becomes still water at depth 8");
        require(lava.mName == "minecraft:lava" && lava.mStates.getInt("liquid_depth") == 3,
                "spreading lava keeps its depth");
        require(level.getScheduledFluidCount() == 3, "every rewritten block waits for its tick");
        require(chunk.isDirty() && !chunk.needsLiquidMigration(), "the chunk is saved once, migrated");
    }
}

int main() {
    try {
        dueOrder();
        duplicates();
        chunkBudget();
        laggingColumn();
        randomClock();
        persistence();
        migration();
        std::cout << "block ticking queue checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
