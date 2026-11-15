#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Misc/PrimedTntActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const float FLOOR = 64.0f;
    const double SIZE = 4.0;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // what one explosion adds to the motion of a tnt, from the per ray reading of the blocks
    Vector3f expectedPush(Level &level, const Vector3f &source, const Vector3f &position) {
        const float half = PrimedTntActor::SIZE * 0.5f;
        const AxisAlignedBB box(position.x - half, position.y, position.z - half, position.x + half,
                                position.y + PrimedTntActor::SIZE, position.z + half);
        const double x = (double) position.x - (double) source.x;
        const double y = (double) position.y - (double) source.y;
        const double z = (double) position.z - (double) source.z;
        const double distance = std::sqrt(x * x + y * y + z * z) / (SIZE * 2.0);
        if (distance > 1.0)
            return Vector3f();

        const Vector3f offset(position.x - source.x, position.y - source.y, position.z - source.z);
        const float length = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        const double impact = (1.0 - distance) * Explosion::getBlockDensity(level, source, box);
        return Vector3f(offset.x / length * (float) impact, offset.y / length * (float) impact,
                        offset.z / length * (float) impact);
    }

    void run() {
        ServerNetworkHandler owner("Tnt blast", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        for (int32_t x = -2; x <= 2; x++) {
            for (int32_t z = -2; z <= 2; z++)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        // a floor, a wall and a slab: targets in the open, hidden and half hidden
        for (int32_t x = -12; x <= 12; x++) {
            for (int32_t z = -12; z <= 12; z++)
                level.setBlockState(x, (int32_t) FLOOR - 1, z, BlockState("minecraft:obsidian"));
        }
        for (int32_t y = 0; y < 3; y++) {
            for (int32_t z = -1; z <= 1; z++)
                level.setBlockState(3, (int32_t) FLOOR + y, z, BlockState("minecraft:obsidian"));
        }
        level.setBlockState(-3, (int32_t) FLOOR, 0, BlockState("minecraft:smooth_stone_slab"));

        // a summoned tnt is the primed one, and nothing pushes it around
        ServerActor *summoned = owner.spawnActor(level, "minecraft:tnt", Vector3f(0.5f, FLOOR, 0.5f));
        require(dynamic_cast<PrimedTntActor *>(summoned) != nullptr, "a summoned tnt is not primed");
        require(!summoned->isPushable(), "a primed tnt is pushable");
        require(summoned->getSize().mWidth == PrimedTntActor::SIZE && summoned->getSize().mHeight == PrimedTntActor::SIZE,
                "a primed tnt is not its 0.98 box");

        std::vector<Vector3f> spots = {
                Vector3f(1.5f, FLOOR, 0.5f),   Vector3f(5.5f, FLOOR, 0.5f),  Vector3f(4.6f, FLOOR + 2.4f, 0.2f),
                Vector3f(-4.5f, FLOOR, 0.5f),  Vector3f(0.5f, FLOOR, 6.5f),  Vector3f(2.5f, FLOOR + 3.0f, -2.5f),
                Vector3f(-6.2f, FLOOR, -3.7f), Vector3f(0.5f, FLOOR, 9.5f),  Vector3f(0.5f, FLOOR + 5.0f, 0.5f),
                Vector3f(5.5f, FLOOR, 1.6f),   Vector3f(-3.5f, FLOOR + 0.6f, 0.5f)};
        // and a lattice all around, steps and pillars in the way: every kind of half hidden target
        for (int32_t x = -5; x <= 5; x += 2) {
            for (int32_t z = -5; z <= 5; z += 2) {
                level.setBlockState(x * 2, (int32_t) FLOOR + ((x + z) & 3), z, BlockState("minecraft:obsidian"));
                for (int32_t y = 0; y <= 4; y += 2)
                    spots.push_back(Vector3f((float) x + 0.37f, FLOOR + (float) y + 0.21f * (float) ((x + 5) % 3),
                                             (float) z + 0.63f));
            }
        }
        std::vector<ServerActor *> targets;
        for (const Vector3f &spot: spots)
            targets.push_back(owner.spawnPrimedTnt(level, spot, Vector3f(), 1000));

        const Vector3f source(0.5f, FLOOR + 0.06125f, 0.5f);
        std::vector<Vector3f> expected;
        bool hidden = false;
        bool open = false;
        for (const Vector3f &spot: spots) {
            expected.push_back(expectedPush(level, source, spot));
            const Vector3f &push = expected.back();
            if (push.x == 0.0f && push.y == 0.0f && push.z == 0.0f)
                hidden = true;
            else
                open = true;
        }
        require(hidden && open, "the scene needs a hidden target and an open one");

        // two explosions sharing one batch, as at the end of a tick
        ExplosionBatch batch(owner);
        for (int round = 0; round < 2; round++) {
            Explosion explosion(owner, level, source, SIZE, summoned, true);
            explosion.setBatch(&batch);
            require(explosion.explodeWithoutBlocks(), "the explosion did not run");
        }

        for (size_t index = 0; index < targets.size(); index++) {
            const Vector3f motion = targets[index]->getMotion();
            const Vector3f once = expected[index];
            const Vector3f twice(once.x + once.x, once.y + once.y, once.z + once.z);
            require(motion.x == twice.x && motion.y == twice.y && motion.z == twice.z,
                    "target " + std::to_string(index) + " was pushed " + std::to_string(motion.x) + " "
                            + std::to_string(motion.y) + " " + std::to_string(motion.z) + ", expected "
                            + std::to_string(twice.x) + " " + std::to_string(twice.y) + " "
                            + std::to_string(twice.z));
        }
        require(summoned->getMotion().x == 0.0f && summoned->getMotion().z == 0.0f, "the source pushed itself");

        // the blocks broken are the same with or without the field: obsidian holds, a dirt pile does not
        for (int32_t x = -1; x <= 1; x++) {
            for (int32_t z = -1; z <= 1; z++)
                level.setBlockState(x, (int32_t) FLOOR + 1, z + 4, BlockState("minecraft:dirt"));
        }
        Explosion breaking(owner, level, Vector3f(0.5f, FLOOR + 1.5f, 2.5f), SIZE, nullptr, true);
        require(breaking.explode(), "the breaking explosion did not run");
        require(level.getBlockState(0, (int32_t) FLOOR + 1, 4).mName == "minecraft:air", "the dirt survived");
        require(level.getBlockState(0, (int32_t) FLOOR - 1, 2).mName == "minecraft:obsidian", "the obsidian broke");

        // a move through water meets the blocks behind it and nothing of the water
        for (int32_t x = -8; x <= -6; x++) {
            for (int32_t y = 0; y < 3; y++)
                level.setBlockState(x, (int32_t) FLOOR + 5 + y, 8, BlockState("minecraft:water"));
        }
        const AxisAlignedBB wet(-7.9f, FLOOR + 5.2f, 8.1f, -6.1f, FLOOR + 6.8f, 8.9f);
        require(level.getCollisionBoxes(wet, nullptr).empty(), "water gave a collision box");
        level.setBlockState(-7, (int32_t) FLOOR + 6, 8, BlockState("minecraft:obsidian"));
        require(level.getCollisionBoxes(wet, nullptr).size() == 1, "the block in the water was missed");
        // the answer from whole sections never calls free a place that holds a box
        require(!level.isFreeOfColliders(wet, nullptr), "a section with a block was called free");
        const AxisAlignedBB sky(-7.9f, 200.2f, 8.1f, 30.1f, 230.8f, 8.9f);
        require(level.isFreeOfColliders(sky, nullptr) && level.getCollisionBoxes(sky, nullptr).empty(),
                "the open sky was not called free");

        // explosions that break nothing settled all at once on every core: far apart, each target hears of
        // one explosion only and gets exactly the push of the per ray reading
        for (int32_t x = 3; x <= 44; x++) {
            for (int32_t z = -2; z <= 2; z++)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }
        std::vector<ExplosionBatch::QuietBlast> blasts;
        std::vector<ServerActor *> pushed;
        std::vector<Vector3f> pushes;
        for (int32_t rank = 0; rank < 20; rank++) {
            const int32_t baseX = 80 + rank * 30;
            for (int32_t x = -9; x <= 9; x++) {
                for (int32_t z = -9; z <= 9; z++)
                    level.setBlockState(baseX + x, (int32_t) FLOOR - 1, z, BlockState("minecraft:obsidian"));
            }
            // a step, a slab and a pillar, placed differently around each source
            level.setBlockState(baseX + 2 + rank % 3, (int32_t) FLOOR, rank % 4 - 2, BlockState("minecraft:obsidian"));
            level.setBlockState(baseX - 3, (int32_t) FLOOR, 1 - rank % 3, BlockState("minecraft:smooth_stone_slab"));
            level.setBlockState(baseX - 1, (int32_t) FLOOR + 1, 3, BlockState("minecraft:obsidian"));
            const Vector3f center((float) baseX + 0.5f, FLOOR + 0.06125f, 0.5f);
            blasts.push_back(ExplosionBatch::QuietBlast{center, SIZE, summoned});
            for (const Vector3f &offset: {Vector3f(4.3f, 0.0f, 0.4f), Vector3f(-5.2f, 0.0f, 0.9f),
                                          Vector3f(-1.4f, 2.2f, 4.6f), Vector3f(1.1f, 0.0f, -6.3f),
                                          Vector3f(6.0f, 1.5f, -2.0f)}) {
                const Vector3f spot(center.x + offset.x, FLOOR + offset.y, center.z + offset.z);
                pushed.push_back(owner.spawnPrimedTnt(level, spot, Vector3f(), 1000));
                pushes.push_back(expectedPush(level, center, spot));
            }
        }
        {
            ExplosionBatch quietBatch(owner);
            quietBatch.settleQuiet(level, blasts);
            // each explosion then ends as it does in a tick: what a shaped block left open is settled there
            for (size_t rank = 0; rank < blasts.size(); rank++) {
                require(quietBatch.isSettled(rank), "a quiet explosion was left whole");
                Explosion explosion(owner, level, blasts[rank].mSource, SIZE, summoned, true);
                explosion.setBatch(&quietBatch);
                explosion.setSettledAs(rank);
                require(explosion.explodeWithoutBlocks(), "a quiet explosion did not end");
            }
        }
        bool partly = false;
        for (size_t index = 0; index < pushed.size(); index++) {
            const Vector3f motion = pushed[index]->getMotion();
            const Vector3f &push = pushes[index];
            require(motion.x == push.x && motion.y == push.y && motion.z == push.z,
                    "quiet target " + std::to_string(index) + " was pushed " + std::to_string(motion.x) + " "
                            + std::to_string(motion.y) + " " + std::to_string(motion.z) + ", expected "
                            + std::to_string(push.x) + " " + std::to_string(push.y) + " " + std::to_string(push.z));
            pushed[index]->setMotion(Vector3f());
        }
        for (const Vector3f &push: pushes)
            partly = partly || (push.x == 0.0f && push.y == 0.0f && push.z == 0.0f);
        require(partly, "the quiet scene needs a hidden target");

        // the blocks that do something to an actor inside them say so: the contact pass skips the others
        for (const char *name: {"minecraft:cactus", "minecraft:web", "minecraft:detector_rail", "minecraft:end_gateway",
                                "minecraft:end_portal", "minecraft:honey_block", "minecraft:portal",
                                "minecraft:powder_snow", "minecraft:sweet_berry_bush"}) {
            const Block *block = VanillaBlocks::fromIdentifier(name);
            require(block != nullptr && block->hearsActorsInside(), std::string(name) + " is not told of actors inside");
        }
        const Block *water = VanillaBlocks::fromIdentifier("minecraft:water");
        require(water == nullptr || !water->hearsActorsInside(), "water is told of actors inside");

        // a tnt whose fuse ends during the actor pass explodes at the end of that pass
        for (ServerActor *target: targets)
            target->setMotion(Vector3f());
        PrimedTntActor *fused = owner.spawnPrimedTnt(level, Vector3f(0.5f, FLOOR, 0.5f), Vector3f(), 1);
        const int64_t fusedId = fused->getUniqueId();
        level.setBlockState(1, (int32_t) FLOOR, 2, BlockState("minecraft:dirt"));
        owner.tickActors();
        require(level.getBlockState(1, (int32_t) FLOOR, 2).mName == "minecraft:air",
                "a tnt going off at the end of the pass broke no block");
        require(owner.getActors().find(fusedId) == owner.getActors().end(), "the exploded tnt is still there");

        // a crowd going off on plain ground in one tick digs it out
        for (int32_t x = 300; x <= 340; x++) {
            for (int32_t z = -10; z <= 10; z++) {
                for (int32_t y = 1; y <= 4; y++)
                    level.setBlockState(x, (int32_t) FLOOR - y, z, BlockState("minecraft:dirt"));
            }
        }
        for (int32_t index = 0; index < 600; index++)
            owner.spawnPrimedTnt(level, Vector3f(305.5f + (float) (index % 30), FLOOR, -7.5f + (float) (index / 30 % 15)),
                                 Vector3f(), 1);
        owner.tickActors();
        int32_t left = 0;
        for (int32_t x = 305; x <= 334; x++) {
            for (int32_t z = -7; z <= 7; z++)
                left += level.getBlockState(x, (int32_t) FLOOR - 1, z).mName == "minecraft:dirt" ? 1 : 0;
        }
        require(left == 0, std::to_string(left) + " blocks of ground survived a crowd of tnt standing on them");
        require(targets[0]->getMotion().x > 0.0f, "the tnt next to the blast was not pushed");

        // many explosions worked out apart: two on each platform, a dirt block next to the source hiding a
        // target from the first one only. Each target gets the push of the per ray reading of the blocks
        // as they stood for each explosion, the far platforms being left to the level
        std::vector<Vector3f> firstPushes;
        // the ticks above left them a little motion of their own
        for (ServerActor *actor: pushed)
            actor->setMotion(Vector3f());
        for (int32_t rank = 0; rank < 20; rank++) {
            const int32_t baseX = 80 + rank * 30;
            level.setBlockState(baseX + 1, (int32_t) FLOOR, 0, BlockState("minecraft:dirt"));
            for (size_t index = (size_t) rank * 5; index < (size_t) rank * 5 + 5; index++)
                firstPushes.push_back(expectedPush(level, blasts[(size_t) rank].mSource, pushed[index]->getPosition()));
        }
        {
            ExplosionBatch apartBatch(owner);
            std::vector<std::unique_ptr<Explosion>> owned;
            std::vector<Explosion *> apart;
            for (int round = 0; round < 2; round++) {
                for (const ExplosionBatch::QuietBlast &blast: blasts) {
                    owned.push_back(std::make_unique<Explosion>(owner, level, blast.mSource, SIZE, summoned, true));
                    owned.back()->setBatch(&apartBatch);
                    apart.push_back(owned.back().get());
                }
            }
            apartBatch.runApart(level, apart);
        }
        bool dirtHid = false;
        for (int32_t rank = 0; rank < 20; rank++) {
            const int32_t baseX = 80 + rank * 30;
            require(level.getBlockState(baseX + 1, (int32_t) FLOOR, 0).mName == "minecraft:air",
                    "the dirt next to an explosion worked out apart survived");
            for (size_t index = (size_t) rank * 5; index < (size_t) rank * 5 + 5; index++) {
                const Vector3f first = firstPushes[index];
                const Vector3f second = expectedPush(level, blasts[(size_t) rank].mSource, pushed[index]->getPosition());
                dirtHid = dirtHid || first.x != second.x || first.y != second.y || first.z != second.z;
                const Vector3f both(first.x + second.x, first.y + second.y, first.z + second.z);
                const Vector3f motion = pushed[index]->getMotion();
                require(motion.x == both.x && motion.y == both.y && motion.z == both.z,
                        "apart target " + std::to_string(index) + " was pushed " + std::to_string(motion.x) + " "
                                + std::to_string(motion.y) + " " + std::to_string(motion.z) + ", expected "
                                + std::to_string(both.x) + " " + std::to_string(both.y) + " "
                                + std::to_string(both.z));
            }
        }
        require(dirtHid, "the dirt hid no target from the first explosion");

        // a plain block drops the same with no tool, the short way, as with one, the long way
        ItemStack stick;
        stick.mDefinition = owner.getItemDefinitions().getDefinition("minecraft:stick");
        stick.mCount = 1;
        for (const char *name: {"minecraft:dirt", "minecraft:cobblestone", "minecraft:oak_planks", "minecraft:obsidian",
                                "minecraft:sand", "minecraft:glass"}) {
            const BlockState state(name);
            const Vector3i place(0, (int32_t) FLOOR + 20, 0);
            for (int round = 0; round < 2; round++) {
                const std::vector<ItemStack> bare = BlockActionHandler::blockDrops(owner, &level, place, state, ItemStack());
                const std::vector<ItemStack> held = BlockActionHandler::blockDrops(owner, &level, place, state, stick);
                require(bare.size() == held.size(), std::string(name) + " drops differ in number with no tool");
                for (size_t index = 0; index < bare.size(); index++)
                    require(bare[index].mDefinition == held[index].mDefinition
                                    && bare[index].mBlockDefinition == held[index].mBlockDefinition
                                    && bare[index].mCount == held[index].mCount,
                            std::string(name) + " drops differ with no tool");
            }
        }

        // items removed during a tick leave the list together, the others keeping their order
        std::vector<std::unique_ptr<ItemActor>> &items = owner.getItemEntities();
        for (const std::unique_ptr<ItemActor> &item: items)
            item->setRemoved(true);
        ItemActorHandler::tickItemActors(owner);
        require(items.empty(), "removed items stayed in the list");
        ItemStack dirt;
        dirt.mDefinition = owner.getItemDefinitions().getDefinition("minecraft:dirt");
        dirt.mBlockDefinition = owner.getBlockDefinitions().getDefinition("minecraft:dirt");
        dirt.mCount = 1;
        std::vector<int64_t> ids;
        for (int32_t index = 0; index < 6; index++)
            ids.push_back(owner.dropItem(level, Vector3f(0.5f + 3.0f * (float) index, FLOOR + 30.0f, 20.5f), dirt,
                                         Vector3f(), 10)
                                  ->getUniqueId());
        items[1]->setRemoved(true);
        items[4]->setRemoved(true);
        ItemActorHandler::tickItemActors(owner);
        require(items.size() == 4 && items[0]->getUniqueId() == ids[0] && items[1]->getUniqueId() == ids[2]
                        && items[2]->getUniqueId() == ids[3] && items[3]->getUniqueId() == ids[5],
                "the items left after a removal are not the ones expected, in their order");

        // an item past the loaded chunks waits there: its fall makes no terrain
        const Vector3f beyond(5000.5f, FLOOR + 30.0f, 0.5f);
        ItemActor *waiting = owner.dropItem(level, beyond, dirt, Vector3f(0.1f, 0.0f, 0.0f), 10);
        require(!level.isChunkResident(5000 >> 4, 0), "dropping an item loaded its chunk");
        ItemActorHandler::tickItemActors(owner);
        require(!level.isChunkResident(5000 >> 4, 0), "a falling item made the terrain under it");
        require(waiting->getPosition().y == beyond.y, "an item fell where no chunk is loaded");
    }
}

int main() {
    try {
        run();
    } catch (const std::exception &error) {
        std::cerr << "tnt blast test failed: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "tnt blast tests passed" << std::endl;
    return 0;
}
