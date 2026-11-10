#include "Network/ServerNetworkHandler.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Level/Block/Actor/EndGatewayBlockActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/Dimension/EndGatewayExit.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/End/Feature/TheEndDecorationFeature.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
    const int32_t MIN_Y = 0;
    const int32_t MAX_Y = 255;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool same(const Vector3i &a, const Vector3i &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    // a level made of the blocks set in it, air everywhere else
    class BlockMap {
    public:
        void set(int32_t x, int32_t y, int32_t z, const BlockState &state) {
            mBlocks[std::make_tuple(x, y, z)] = state;
        }

        void fill(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                  const BlockState &state) {
            for (int32_t x = minX; x <= maxX; x++) {
                for (int32_t y = minY; y <= maxY; y++) {
                    for (int32_t z = minZ; z <= maxZ; z++)
                        set(x, y, z, state);
                }
            }
        }

        EndGatewayExit::BlockReader reader() const {
            return [this](int32_t x, int32_t y, int32_t z) -> const BlockState * {
                const auto found = mBlocks.find(std::make_tuple(x, y, z));
                return found == mBlocks.end() ? nullptr : &found->second;
            };
        }

        EndSpikes::BlockSink sink() {
            return [this](int32_t x, int32_t y, int32_t z, const BlockState &state) { set(x, y, z, state); };
        }

    private:
        std::map<std::tuple<int32_t, int32_t, int32_t>, BlockState> mBlocks;
    };

    const BlockState &endStone() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    const BlockState &bedrock() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    void checkNbt() {
        BlockDefinitionRegistry blocks;
        ItemDefinitionRegistry items;
        PacketCodecContext context(blocks, items);

        EndGatewayBlockActor gateway;
        gateway.setPosition(Vector3i(96, 75, 0));
        gateway.setExit(Vector3i(1010, 70, -3));
        for (int32_t tick = 0; tick < 5; tick++)
            gateway.startTick();

        const Tag saved = gateway.saveNbt();
        require(saved.getInt("Age", -1) == 5, "The age is saved as an int");
        const Tag *exit = saved.get("ExitPortal");
        require(exit != nullptr && exit->isList() && exit->getListType() == Tag::Type::Int && exit->size() == 3,
                "The exit is saved as a list of three ints");
        require(exit->getList()[0].asInt() == 1010 && exit->getList()[1].asInt() == 70
                        && exit->getList()[2].asInt() == -3,
                "The exit list holds x, y and z");
        require(saved.getByte("EndGatewayBadPosChecked", 0) == 1, "Every save marks the exit as checked");

        const Tag spawn = gateway.getSpawnCompound();
        require(spawn.getString("id", std::string()) == "EndGateway" && spawn.getInt("x", 0) == 96
                        && spawn.getInt("y", 0) == 75,
                "The client gets the gateway's id and position");
        require(spawn.get("ExitPortal") != nullptr && spawn.getInt("Age", -1) == 5,
                "The client gets the gateway with its saved data");

        EndGatewayBlockActor loaded;
        loaded.setPosition(Vector3i(96, 75, 0));
        loaded.loadNbt(saved, context);
        require(same(loaded.getExit(), Vector3i(1010, 70, -3)) && loaded.getAge() == 5,
                "A saved gateway loads back its exit and age");
        require(loaded.isExitCheckPending() && !loaded.isExitSearchPending() && !loaded.isBadPosition(),
                "A gateway loaded with an exit checks it without searching again");

        EndGatewayBlockActor empty;
        empty.setPosition(Vector3i(96, 75, 0));
        empty.loadNbt(Tag::ofCompound(), context);
        require(empty.isExitSearchPending() && !empty.hasExit(), "A gateway without an exit searches for one");

        Tag zero = saved;
        Tag origin = Tag::ofList(Tag::Type::Int);
        for (int32_t i = 0; i < 3; i++)
            origin.addToList(Tag::ofInt(0));
        zero.put("ExitPortal", origin);
        EndGatewayBlockActor atOrigin;
        atOrigin.loadNbt(zero, context);
        require(atOrigin.isExitSearchPending(), "An exit at the origin counts as no exit");

        Tag old = Tag::ofCompound();
        Tag oldExit = Tag::ofList(Tag::Type::Int);
        oldExit.addToList(Tag::ofInt(40));
        oldExit.addToList(Tag::ofInt(50));
        oldExit.addToList(Tag::ofInt(9));
        old.put("ExitPortal", oldExit);
        EndGatewayBlockActor legacy;
        legacy.setPosition(Vector3i(96, 75, 0));
        legacy.loadNbt(old, context);
        require(legacy.isBadPosition() && legacy.isExitCheckPending(),
                "An unchecked exit near the origin of a gateway near the origin is found again");

        old.putBool("EndGatewayBadPosChecked", true);
        EndGatewayBlockActor checked;
        checked.setPosition(Vector3i(96, 75, 0));
        checked.loadNbt(old, context);
        require(!checked.isBadPosition(), "A checked exit is kept");

        require(EndGatewayBlockActor::isBadPosition(Vector3i(40, 50, 9), Vector3i(96, 75, 0)),
                "Exit 99 blocks and gateway 299 blocks from the origin, by their summed axes");
        require(!EndGatewayBlockActor::isBadPosition(Vector3i(40, 50, 10), Vector3i(96, 75, 0)),
                "An exit 100 blocks away is not a bad position");
        require(!EndGatewayBlockActor::isBadPosition(Vector3i(40, 50, 9), Vector3i(200, 75, 25)),
                "A gateway 300 blocks away is not a bad position");
    }

    void checkCooldown() {
        EndGatewayBlockActor gateway;
        require(gateway.startTick() && !gateway.needsClientUpdate(), "A new gateway lets entities through");

        gateway.triggerCooldown();
        require(gateway.getCooldown() == EndGatewayBlockActor::COOLDOWN_TICKS && gateway.needsClientUpdate(),
                "A teleport starts a 20 tick cooldown the client is told about");

        for (int32_t tick = 1; tick < EndGatewayBlockActor::COOLDOWN_TICKS; tick++) {
            require(!gateway.startTick(), "Nothing goes through while the gateway cools down");
            require(!gateway.needsClientUpdate(), "The cooldown runs down without updates");
        }

        require(!gateway.startTick() && gateway.getCooldown() == 0, "The last cooldown tick still blocks");
        require(gateway.needsClientUpdate(), "The client is told when the cooldown ends");
        require(gateway.startTick(), "The gateway opens again after its cooldown");

        while (gateway.getAge() < EndGatewayBlockActor::SPAWN_BEAM_TICKS - 1) {
            gateway.startTick();
            require(!gateway.needsClientUpdate(), "The spawn beam lasts 200 ticks without updates");
        }
        gateway.startTick();
        require(gateway.getAge() == EndGatewayBlockActor::SPAWN_BEAM_TICKS && gateway.needsClientUpdate(),
                "The client is told when the spawn beam ends");

        const Vector3f target = EndGatewayBlockActor::getTeleportTarget(Vector3i(5, 60, -7));
        require(target.x == 5.5f && target.y == 61.0f && target.z == -6.5f,
                "An entity lands one block above the exit, centred");
    }

    void checkRoughExit() {
        const auto band = [](int32_t chunkX, int32_t chunkZ) {
            (void) chunkZ;
            return chunkX >= 60 && chunkX <= 70;
        };
        require(same(EndGatewayExit::findRoughExit(Vector3i(96, 75, 0), band), Vector3i(960, 0, 0)),
                "The exit walks back to the inner edge of the land 1024 blocks out");

        const auto nothing = [](int32_t chunkX, int32_t chunkZ) {
            (void) chunkX;
            (void) chunkZ;
            return false;
        };
        require(same(EndGatewayExit::findRoughExit(Vector3i(96, 75, 0), nothing), Vector3i(1280, 0, 0)),
                "Without land the exit moves out 16 steps of 16 blocks");

        const auto everywhere = [](int32_t chunkX, int32_t chunkZ) {
            (void) chunkX;
            (void) chunkZ;
            return true;
        };
        require(same(EndGatewayExit::findRoughExit(Vector3i(0, 75, -96), everywhere), Vector3i(0, 0, -768)),
                "Over land the exit moves back at most 16 steps");

        const auto outer = [](int32_t chunkX, int32_t chunkZ) {
            return (int64_t) chunkX * chunkX + (int64_t) chunkZ * chunkZ >= 66LL * 66LL;
        };
        const Vector3i diagonal = EndGatewayExit::findRoughExit(EndDragonFight::getGatewayPosition(3), outer);
        require(diagonal.y == 0 && outer(diagonal.x >> 4, diagonal.z >> 4)
                        && !outer((int32_t) std::floor((float) diagonal.x * 0.98f) >> 4,
                                  (int32_t) std::floor((float) diagonal.z * 0.98f) >> 4),
                "A gateway off the axes finds the edge of the land in its direction");

        TheEndGenerator generator(0);
        const Vector3i exit = EndGatewayExit::findRoughExit(Vector3i(96, 75, 0), [&](int32_t chunkX, int32_t chunkZ) {
            return generator.getTerrainTop(chunkX, chunkZ) != 0;
        });
        require(generator.getTerrainTop(exit.x >> 4, exit.z >> 4) != 0,
                "The generated outer islands give the exit land to stand on");
        require(generator.getTerrainTop(0, 0) > 0 && generator.getTerrainTop(40, 0) == 0,
                "The main island has terrain and the void around it has none");
    }

    void checkSpawnSearch() {
        BlockMap map;
        map.fill(98, 60, -2, 102, 60, 2, endStone());
        const Vector3i spawn = EndGatewayExit::findSpawn(map.reader(), Vector3i(100, 0, 0), true, 1, false, MIN_Y,
                                                         MAX_Y);
        require(same(spawn, Vector3i(100, 60, 0)), "The closest enclosed end stone top is the landing");

        const Vector3i edge = EndGatewayExit::findSpawn(map.reader(), Vector3i(110, 60, 0), true, 1, false, MIN_Y,
                                                        MAX_Y);
        require(same(edge, Vector3i(101, 60, 0)), "A top with air beside it loses to an enclosed one");

        BlockMap ledge;
        ledge.set(100, 60, 0, endStone());
        require(same(EndGatewayExit::findSpawn(ledge.reader(), Vector3i(105, 0, 0), true, 1, false, MIN_Y, MAX_Y),
                     Vector3i(100, 60, 0)),
                "Without an enclosed top the closest top is kept");

        BlockMap empty;
        require(EndGatewayExit::isZero(
                        EndGatewayExit::findSpawn(empty.reader(), Vector3i(100, 0, 0), true, 1, false, MIN_Y, MAX_Y)),
                "Nothing to land on gives no landing");

        TheEndDecorationFeature::placeGateway(100, 70, 0, map.sink());
        const Vector3i under = EndGatewayExit::findSpawn(map.reader(), Vector3i(100, 60, 0), false, 1, false, MIN_Y,
                                                         MAX_Y);
        require(same(under, Vector3i(100, 60, 0)), "The frame of a gateway is skipped down to the land under it");
    }

    void checkTallestBlock() {
        BlockMap map;
        map.fill(98, 60, -2, 102, 60, 2, endStone());
        map.set(102, 65, 1, endStone());
        map.set(101, 65, -1, endStone());
        require(same(EndGatewayExit::findTallestBlock(map.reader(), Vector3i(100, 60, 0), 16, true, false, MAX_Y + 1),
                     Vector3i(101, 65, -1)),
                "The tallest block wins, the first column found keeps a tie");

        map.fill(100, 61, 0, 100, 80, 0, bedrock());
        require(same(EndGatewayExit::findTallestBlock(map.reader(), Vector3i(100, 60, 0), 16, true, false, MAX_Y + 1),
                     Vector3i(100, 80, 0)),
                "The exit search counts bedrock");
        require(same(EndGatewayExit::findTallestBlock(map.reader(), Vector3i(100, 60, 0), 5, false, true, MAX_Y + 1),
                     Vector3i(101, 65, -1)),
                "The landing search skips its centre and bedrock");

        BlockMap empty;
        require(EndGatewayExit::isZero(
                        EndGatewayExit::findTallestBlock(empty.reader(), Vector3i(100, 60, 0), 16, true, false, 256)),
                "An empty area has no tallest block");
    }

    void checkExitChecks() {
        BlockMap map;
        map.fill(98, 60, -2, 102, 60, 2, endStone());
        require(!EndGatewayExit::isExitBlocked(map.reader(), Vector3i(100, 61, 0)), "Air over land is a good exit");
        require(EndGatewayExit::isExitBlocked(map.reader(), Vector3i(100, 60, 0)), "An exit in a block is moved");
        require(EndGatewayExit::isExitBlocked(map.reader(), Vector3i(110, 61, 0)), "An exit over the void is moved");

        TheEndDecorationFeature::placeGateway(100, 70, 0, map.sink());
        require(EndGatewayExit::isExitBlocked(map.reader(), Vector3i(100, 70, 0)),
                "A gateway is no place to land, so the gateway leading back is moved");
    }

    void checkIsland() {
        require(EndGatewayExit::islandSeed(Vector3i(1000, 75, -20)) == 3033158844u
                        && EndGatewayExit::islandSeed(Vector3i(0, 0, 0)) == 1148853900u,
                "The island seed combines the hashes of x, y and z");

        BlockMap map;
        std::mt19937 random(EndGatewayExit::islandSeed(Vector3i(1000, 75, -20)));
        TheEndDecorationFeature::placeIsland(random, 1000, 75, -20, map.sink());
        const Vector3i landing = EndGatewayExit::findSpawn(map.reader(), Vector3i(1000, 75, -20), true, 1, false, MIN_Y,
                                                           MAX_Y);
        require(same(landing, Vector3i(1000, 75, -20)), "A raised island gives the exit its landing");
    }

    std::filesystem::path storageDirectory(const std::string &name) {
        return std::filesystem::temp_directory_path()
               / ("falcon-" + name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    }

    // an End saved to disk with no player in it, so no column is loaded for actors
    struct EndFixture {
        ServerNetworkHandler mOwner{"End gateway", "", 1};
        World &mWorld = mOwner.getWorlds().getDefault();
        Level &mEnd = mWorld.getLevel(DimensionType::TheEnd);
        EndDragonFight &mFight = mWorld.getDragonFight();
        std::filesystem::path mDirectory;

        explicit EndFixture(const std::string &name) : mDirectory(storageDirectory(name)) {
            require(mEnd.openStorage(mDirectory.string()), "Could not open the End storage");
        }

        ~EndFixture() {
            mEnd.closeStorage();
            std::error_code error;
            std::filesystem::remove_all(mDirectory, error);
        }

        void loadAround(const Vector3i &center, int32_t reach) {
            for (int32_t chunkX = (center.x - reach) >> 4; chunkX <= (center.x + reach) >> 4; chunkX++) {
                for (int32_t chunkZ = (center.z - reach) >> 4; chunkZ <= (center.z + reach) >> 4; chunkZ++) {
                    if (!mEnd.isChunkPopulated(chunkX, chunkZ))
                        mEnd.insertChunk(LevelChunk(chunkX, chunkZ)).setPopulated(true);
                }
            }
        }

        // end stone under the gateway east of the island, where the gateway back lands
        void placeLanding() {
            for (int32_t x = 88; x <= 104; x++) {
                for (int32_t z = -8; z <= 8; z++)
                    mEnd.setBlock(Vector3i(x, 60, z), endStone(), false);
            }
        }

        EndGatewayBlockActor &placeGateway(const Vector3i &position, const Vector3i &exit) {
            mEnd.setBlock(position, VanillaBlocks::END_GATEWAY().toBlockState(), false);
            EndGatewayBlockActor &gateway = mEnd.getBlockActors().getOrCreate<EndGatewayBlockActor>(position);
            gateway.setExit(exit);
            return gateway;
        }

        std::vector<Tag> storedGateways(const Vector3i &center, int32_t reach) {
            std::vector<Tag> found;
            for (int32_t chunkX = (center.x - reach) >> 4; chunkX <= (center.x + reach) >> 4; chunkX++) {
                for (int32_t chunkZ = (center.z - reach) >> 4; chunkZ <= (center.z + reach) >> 4; chunkZ++) {
                    for (const Tag &blockActor: mEnd.loadBlockEntities(chunkX, chunkZ)) {
                        if (blockActor.getString("id", std::string()) == EndGatewayBlockActor::BLOCK_ACTOR_ID)
                            found.push_back(blockActor);
                    }
                }
            }
            return found;
        }

        bool isStoredEntity(const Vector3f &position, const std::string &identifier, int64_t uniqueId) {
            const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
            const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
            for (const Tag &entity: mEnd.loadEntities(chunkX, chunkZ)) {
                if (entity.getString("identifier", std::string()) == identifier
                    && entity.getLong("UniqueID", 0) == uniqueId)
                    return true;
            }
            return false;
        }
    };

    Vector3i storedPosition(const Tag &blockActor) {
        return Vector3i(blockActor.getInt("x", 0), blockActor.getInt("y", 0), blockActor.getInt("z", 0));
    }

    Vector3i storedExit(const Tag &blockActor) {
        const Tag *exit = blockActor.get("ExitPortal");
        if (exit == nullptr || !exit->isList() || exit->size() != 3)
            return Vector3i(0, 0, 0);

        const std::vector<Tag> &values = exit->getList();
        return Vector3i(values[0].asInt(), values[1].asInt(), values[2].asInt());
    }

    bool onLanding(const Vector3i &position) {
        return position.x >= 88 && position.x <= 104 && position.z >= -8 && position.z <= 8;
    }

    void checkGatewaysSaved() {
        EndFixture end("end-gateway-save");
        const Vector3i gateway(96, 75, 0);
        const Vector3i rough = EndDragonFight::findRoughExit(end.mEnd, gateway);
        end.loadAround(gateway, 80);
        end.loadAround(rough, 80);
        end.placeLanding();

        Tag chest = Tag::ofCompound();
        chest.putString("id", "Chest");
        chest.putInt("x", rough.x);
        chest.putInt("y", 10);
        chest.putInt("z", rough.z);
        end.mEnd.saveBlockEntities(rough.x >> 4, rough.z >> 4, {chest});

        end.mFight.requestGatewayExit(end.mEnd, gateway, true);
        end.mFight.tick(end.mOwner, end.mEnd);
        require(end.mFight.getGatewayRequestCount() == 0, "A gateway whose ends are loaded gets its exit at once");

        const std::vector<Tag> placed = end.storedGateways(gateway, 0);
        require(placed.size() == 1 && !EndGatewayExit::isZero(storedExit(placed.front())),
                "A new gateway no player stands by is saved with its exit");

        const std::vector<Tag> back = end.storedGateways(rough, 80);
        require(back.size() == 1 && onLanding(storedExit(back.front())), "The gateway back is saved and leads home");
        require(end.mEnd.getBlockActors().find<EndGatewayBlockActor>(gateway) == nullptr
                        && end.mEnd.getBlockActors().find<EndGatewayBlockActor>(storedPosition(back.front()))
                                   == nullptr,
                "Gateways in columns without a player are left on disk");

        bool chestKept = false;
        for (const Tag &blockActor: end.mEnd.loadBlockEntities(rough.x >> 4, rough.z >> 4))
            chestKept = chestKept || blockActor.getString("id", std::string()) == "Chest";
        require(chestKept, "Writing the far end back keeps the block actors already saved there");
    }

    void checkReturnGatewayFromDisk() {
        EndFixture end("end-gateway-return");
        const Vector3i gateway(96, 75, 0);
        const Vector3i exit(1000, 61, 0);
        const Vector3i returnGateway(1000, 71, 0);
        const Vector3i offLanding(100, 61, 14);
        end.loadAround(gateway, 80);
        end.loadAround(exit, 80);
        end.placeLanding();
        end.placeGateway(gateway, exit);

        EndGatewayBlockActor saved;
        saved.setPosition(returnGateway);
        saved.setExit(offLanding);
        end.mEnd.saveBlockEntities(returnGateway.x >> 4, returnGateway.z >> 4, {saved.saveWithPosition()});

        end.mFight.requestGatewayCheck(gateway, exit);
        end.mFight.tick(end.mOwner, end.mEnd);
        require(end.mFight.getGatewayRequestCount() == 0, "A check whose ends are loaded is done at once");

        const std::vector<Tag> back = end.storedGateways(returnGateway, 0);
        require(back.size() == 1 && onLanding(storedExit(back.front())),
                "The check finds the gateway back on disk and moves its exit onto land");
    }

    void checkEntityThroughUnloadedEnd() {
        EndFixture end("end-gateway-entity");
        const Vector3i gateway(96, 75, 0);
        end.loadAround(gateway, 16);
        EndGatewayBlockActor &portal = end.placeGateway(gateway, Vector3i(1000, 60, 0));

        ServerActor *cow = end.mOwner.spawnActor(end.mEnd, "minecraft:cow", Vector3f(96.5f, 75.0f, 0.5f));
        require(cow != nullptr, "Could not spawn the cow");
        const int64_t uniqueId = cow->getUniqueId();

        portal.tick(end.mOwner);
        require(portal.getCooldown() > 0, "The cow goes through the gateway");
        require(end.isStoredEntity(EndGatewayBlockActor::getTeleportTarget(portal.getExit()), "minecraft:cow",
                                   uniqueId),
                "An entity sent to a column nothing loads waits in that column's saved entities");
    }

    void checkItemThroughGateway() {
        EndFixture end("end-gateway-item");
        const Vector3i gateway(96, 75, 0);
        const Vector3i exit(1000, 60, 0);
        end.loadAround(gateway, 16);
        EndGatewayBlockActor &portal = end.placeGateway(gateway, exit);
        const Vector3f target = EndGatewayBlockActor::getTeleportTarget(exit);
        end.mWorld.getActorLoadedChunks(DimensionType::TheEnd)
                .insert(((int64_t) ((int32_t) std::floor(target.x) >> 4) << 32)
                        | (uint32_t) ((int32_t) std::floor(target.z) >> 4));

        const ItemStack diamond = end.mOwner.createItemStack("minecraft:diamond", 1);
        ItemActor *item = end.mOwner.dropItem(end.mEnd, Vector3f(96.5f, 75.1f, 0.5f), diamond,
                                              Vector3f(0.0f, 0.0f, 0.0f), 0);
        require(item != nullptr, "Could not drop the item");

        portal.tick(end.mOwner);
        require(portal.getCooldown() > 0 && item->getPosition().x == target.x && item->getPosition().y == target.y
                        && item->getPosition().z == target.z,
                "A dropped item goes through the gateway");

        while (portal.getCooldown() > 0)
            portal.tick(end.mOwner);
        const Vector3i unloaded(2000, 60, 0);
        portal.setExit(unloaded);
        ItemActor *second = end.mOwner.dropItem(end.mEnd, Vector3f(96.5f, 75.1f, 0.5f), diamond,
                                                Vector3f(0.0f, 0.0f, 0.0f), 0);
        require(second != nullptr, "Could not drop the second item");
        const int64_t uniqueId = second->getUniqueId();

        portal.tick(end.mOwner);
        bool kept = false;
        for (const std::unique_ptr<ItemActor> &entry: end.mOwner.getItemEntities())
            kept = kept || entry->getUniqueId() == uniqueId;
        require(!kept
                        && end.isStoredEntity(EndGatewayBlockActor::getTeleportTarget(unloaded), "minecraft:item",
                                              uniqueId),
                "An item sent to a column nothing loads waits in that column's saved entities");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        checkNbt();
        checkCooldown();
        checkRoughExit();
        checkSpawnSearch();
        checkTallestBlock();
        checkExitChecks();
        checkIsland();
        checkGatewaysSaved();
        checkReturnGatewayFromDisk();
        checkEntityThroughUnloadedEnd();
        checkItemThroughGateway();
    } catch (const std::exception &exception) {
        std::cout << "End gateway check failed: " << exception.what() << '\n';
        return 1;
    }

    std::cout << "End gateway checks passed" << std::endl;
    return 0;
}
