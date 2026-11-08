#include "Network/Handler/ChunkStreamHandler.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/ClientSideGeneration.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/Storage/LevelStorage.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float value, float expected) {
        return std::fabs(value - expected) < 1e-5f;
    }

    void serverBuildRatio() {
        using ClientSideGeneration::computeServerBuildRatio;

        require(near(computeServerBuildRatio(32, 8), 1.0f), "a client below nine chunks leaves everything to the server");
        require(near(computeServerBuildRatio(32, 28), 0.0f), "a client handling 28 chunks takes everything it can");
        require(near(computeServerBuildRatio(10, 12), 5.0f / 23.0f), "the server share caps the ratio");
        require(near(computeServerBuildRatio(32, 20), 8.0f / 23.0f), "the client share caps the ratio");
        require(near(computeServerBuildRatio(32, 12), 1.0f), "a gap of 16 chunks on a large server keeps the larger share");
        require(near(computeServerBuildRatio(16, 9), 19.0f / 23.0f - 11.0f / 23.0f),
                "a small gap on a small server takes the difference of the shares");
        require(near(computeServerBuildRatio(32, 4), 1.0f), "four chunks is below the nine chunk floor");
    }

    void serverBuildRadius() {
        using ClientSideGeneration::computeServerBuildRadius;
        using ClientSideGeneration::computeViewRadius;

        require(computeViewRadius(32, 4) == 38, "the view reaches six chunks past the chunk radius");
        require(computeViewRadius(2, 8) == 14, "the view never shrinks below the tick distance");
        require(computeServerBuildRadius(0.0f, 38, 4) == 5, "no share falls back to five chunks");
        require(computeServerBuildRadius(0.0f, 38, 8) == 8, "no share falls back to the tick distance");
        require(computeServerBuildRadius(0.5f, 38, 4) == 19, "half the view");
        require(computeServerBuildRadius(0.1f, 38, 4) == 5, "never below five chunks");
        require(computeServerBuildRadius(1.0f, 16, 4) == 16, "the whole view");
    }

    void neighbourhood() {
        using ClientSideGeneration::isNeighbourOffset;

        int count = 0;
        for (int32_t dx = -8; dx <= 8; ++dx) {
            for (int32_t dz = -8; dz <= 8; ++dz) {
                if (isNeighbourOffset(dx, dz))
                    ++count;
            }
        }
        require(count == 13 * 13 - 4, "the neighbourhood is the 13 x 13 square without its corners");
        require(!isNeighbourOffset(6, 6) && isNeighbourOffset(6, 5) && !isNeighbourOffset(7, 0),
                "corners and chunks past six are out");
    }

    void decision() {
        std::set<std::pair<int32_t, int32_t>> saved;
        saved.insert({30, 0});

        ClientSideGeneration::BuildContext context;
        context.mBuildRadius = 5;
        context.mPlayerCount = 1;
        context.mPlayerChunks.emplace_back(0, 0);
        context.mIsSaved = [&saved](int32_t x, int32_t z) {
            return saved.count({x, z}) != 0;
        };

        using ClientSideGeneration::isServerBuilt;
        require(isServerBuilt(11, 0, context), "the edge of the player margin stays on the server");
        require(!isServerBuilt(12, 0, context), "past the margin the client builds");
        require(!isServerBuilt(8, 8, context), "the margin is a circle");
        require(isServerBuilt(30, 0, context), "a saved chunk stays on the server");
        require(isServerBuilt(24, 0, context), "a chunk six away from a saved one stays on the server");
        require(isServerBuilt(25, 5, context), "the neighbourhood reaches diagonals inside its circle");
        require(!isServerBuilt(24, 6, context), "the neighbourhood has no corners");
        require(!isServerBuilt(23, 0, context), "seven chunks from a saved one the client builds");

        ClientSideGeneration::TickingAreaBounds area;
        area.mMinChunkX = 100;
        area.mMaxChunkX = 103;
        area.mMinChunkZ = 0;
        area.mMaxChunkZ = 3;
        context.mTickingAreas.push_back(area);
        require(isServerBuilt(107, 1, context), "a ticking area keeps the chunks six past its half width");
        require(!isServerBuilt(109, 1, context), "further from the ticking area the client builds");

        context.mIsForced = [](int32_t x, int32_t z) {
            return x == 50 && z == 50;
        };
        require(isServerBuilt(50, 50, context), "the parity guard keeps a chunk on the server");

        context.mPlayerCount = 0;
        require(isServerBuilt(500, 500, context), "with nobody in the level everything is built by the server");
    }

    void savedChunkList() {
        ClientSideGeneration::SavedChunkList list;
        require(list.empty(), "a new list is empty");
        list.add(3, 4);
        list.add(-1, 7);
        list.add(3, 4);
        require(list.size() == 2, "a chunk sent twice is listed once");

        const std::vector<Vector2i> drained = list.drain();
        require(drained.size() == 2 && drained[0].x == 3 && drained[0].y == 4 && drained[1].x == -1
                && drained[1].y == 7, "the list keeps the order the chunks were sent in");
        require(list.empty(), "draining empties the list");
        list.add(3, 4);
        require(list.size() == 1, "a drained chunk can be listed again");
    }

    void publisherSchedule() {
        using Clock = std::chrono::steady_clock;
        const Clock::time_point now = Clock::now();

        ChunkStreamState state;
        require(ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(0, 64, 0), 160, now),
                "the first update goes out at once");

        state.mPublished = true;
        state.mPublisherPosition = Vector3i(0, 64, 0);
        state.mPublisherRadius = 160;
        require(!ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(15, 64, 0), 160, now),
                "moving less than sixteen blocks keeps the update");
        require(ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(16, 64, 0), 160, now),
                "sixteen blocks move the update");
        require(ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(0, 64, 0), 176, now),
                "a new radius moves the update");

        state.mSavedChunks.add(1, 1);
        state.mChunkSentSinceUpdate = true;
        state.mLastChunkSent = now - std::chrono::milliseconds(100);
        require(!ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(0, 64, 0), 160, now),
                "the update waits for the burst of chunks to end");
        require(!ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(100, 64, 0), 160, now),
                "moving does not skip the wait");
        state.mLastChunkSent = now - std::chrono::milliseconds(251);
        require(ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(0, 64, 0), 160, now),
                "listed chunks are published 251 ms after the last one");

        state.mSavedChunks.clear();
        state.mChunkSentSinceUpdate = false;
        state.mForcePublisherMove = true;
        require(ChunkStreamHandler::isPublisherUpdateDue(state, Vector3i(0, 64, 0), 160, now),
                "a teleport moves the update without travelling");
    }

    void mapPixels() {
        Tag map = Tag::ofCompound();
        map.putLong("mapId", 5);

        std::vector<MapPixel> pixels(3);
        pixels[0].mPixel = (int32_t) 0xff336699;
        pixels[0].mIndex = 129;
        pixels[1].mPixel = 0;
        pixels[1].mIndex = 2;
        pixels[2].mPixel = 7;
        pixels[2].mIndex = ClientSideGeneration::MAP_PIXEL_COUNT;

        require(ClientSideGeneration::applyClientMapPixels(map, pixels), "a new pixel changes the map");
        const Tag *colors = map.get("colors");
        require(colors != nullptr && colors->asByteArray().size() == 65536, "the map keeps four bytes per pixel");
        const std::vector<int8_t> &bytes = colors->asByteArray();
        require((uint8_t) bytes[129 * 4] == 0x99 && (uint8_t) bytes[129 * 4 + 1] == 0x66
                && (uint8_t) bytes[129 * 4 + 2] == 0x33 && (uint8_t) bytes[129 * 4 + 3] == 0xff,
                "the colour is stored as the game lays it out");
        require(bytes[2 * 4] == 0, "a blank pixel is ignored");
        require(!ClientSideGeneration::applyClientMapPixels(map, pixels), "the same pixels change nothing");
    }

    void savedChunkPersistence(const std::filesystem::path &directory) {
        {
            LevelStorage storage;
            require(storage.open(directory.string(), "World"), "could not create the storage");
            require(!storage.hasChunk(3, -2), "an empty world holds no chunk");

            LevelChunk chunk(3, -2);
            chunk.setPopulated(true);
            require(storage.saveChunk(chunk), "could not save the chunk");
            require(storage.hasChunk(3, -2), "a saved chunk is found");
            require(!storage.hasChunk(-2, 3), "other chunks are not");

            Tag map = Tag::ofCompound();
            map.putLong("mapId", -9);
            require(storage.saveMapData(-9, map), "could not save the map");
            storage.writeLevelDat("World", 0, 80, 0, 0, 1, 12345, 0, false, false, 0);
        }
        {
            LevelStorage storage;
            require(storage.open(directory.string(), "World"), "could not reopen the storage");
            require(storage.hasChunk(3, -2), "the saved chunk survives a restart");
            Tag map;
            require(storage.loadMapData(-9, map) && map.getLong("mapId") == -9,
                    "the map survives a restart");
        }
        {
            Level level("World", 4, 12345);
            require(level.openStorage(directory.string()), "could not open the level");
            require(level.isChunkSaved(3, -2), "the level reads saved chunks from storage");
            require(!level.isChunkSaved(4, 4), "the level does not invent chunks");
            require(level.isChunkSaved(3, -2) && !level.isChunkSaved(4, 4), "cached answers do not change");
            level.closeStorage();
        }
    }

    void structureGuard() {
        const StructureFeature *village = StructureFeature::get("village");
        require(village != nullptr, "villages are registered");

        Level level("Guard", 4, 12345);
        StructureChunkPos start;
        require(village->findNearest(level.getGenerator(), 0, 0, start), "no village near the origin");

        const std::vector<const StructureFeature *> features = {village};
        require(level.isNearStructure(features, start.x, start.z), "the start chunk of a village is guarded");
        require(level.isNearStructure(features, start.x + ClientSideGeneration::STRUCTURE_GUARD_RADIUS, start.z),
                "chunks within the guard radius are guarded");
        require(!level.isNearStructure({}, start.x, start.z), "without structures nothing is guarded");
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-csg-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        BiomeChunkGenDataRegistry::initialize();
        BlockPaletteRegistry::getInstance().initialize();

        serverBuildRatio();
        serverBuildRadius();
        neighbourhood();
        decision();
        savedChunkList();
        publisherSchedule();
        mapPixels();
        savedChunkPersistence(directory);
        structureGuard();

        std::filesystem::remove_all(directory);
        std::cout << "Client side generation checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
