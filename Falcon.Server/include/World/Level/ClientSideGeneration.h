#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Types/MapPixel.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class ChunkGenerator;
class StructureFeature;
class Tag;

/**
 * Client side generation as BDS 1.26.61 runs it, recovered from the Windows client 26.60 which hosts the same
 * server code. With client-side-chunk-generation-enabled, the chunk view a player keeps on the server only loads
 * the chunks the server has to own (ChunkSource::createEmptyView, generate mode 1, sub_1430014E0). The others
 * are never loaded, so they are never sent, and a client that announced CompatibleWithClientSideChunkGen
 * generates them itself. Every chunk the server does send is listed back to the client in the next
 * NetworkChunkPublisherUpdatePacket.
 */
namespace ClientSideGeneration {
    // sub_14381FD00: a player keeps the chunks within this many chunks past its build radius on the server.
    constexpr int32_t PLAYER_MARGIN = 6;

    // sub_1430014E0 and sub_14381FD00 walk the 13 x 13 chunks around a chunk and keep the offsets whose squared
    // distance is below this, which leaves out only the four corners.
    constexpr int32_t NEIGHBOUR_RADIUS = 6;
    constexpr float NEIGHBOUR_REACH_SQUARED = 67.76666f;

    // sub_14381FD00 never goes below this build radius.
    constexpr int32_t MIN_BUILD_RADIUS = 5;

    // sub_14102BCD0 caps the server radius it weighs the client against.
    constexpr int32_t MAX_WEIGHED_RADIUS = 28;

    // sub_14255F580: with a chunk sent since the last update, the next update waits until this long after the
    // last chunk, so it carries every chunk of the burst.
    constexpr int64_t PUBLISHER_DELAY_NANOSECONDS = 251000000;

    // sub_14102EC10 (dword_14F084AA8): blocks a player moves before its publisher update follows it.
    constexpr int32_t PUBLISHER_MOVE_THRESHOLD = 16;

    // sub_1406C99E0: the storage existence cache is emptied once it holds this many chunks.
    constexpr size_t SAVED_CHUNK_CACHE_LIMIT = 30000;

    // How far from the start chunk of a structure the parity guard keeps chunks on the server. Jigsaw structures
    // place pieces up to 80 blocks from their start, and the largest pieces reach a chunk further.
    constexpr int32_t STRUCTURE_GUARD_RADIUS = 7;

    /**
     * The share of the view a CSG client leaves to the server (sub_14102BCD0, stored at ServerPlayer+2852), from
     * the server view distance and the largest radius the client says its hardware handles.
     */
    float computeServerBuildRatio(int32_t serverViewDistance, int32_t clientMaxRadius);

    /**
     * The radius around a player the server builds itself (sub_14381FD00). viewRadius is the radius of the
     * player's chunk view, max(chunk radius, tick distance) + 6 (sub_14102BCD0, sub_1402E1410).
     */
    int32_t computeServerBuildRadius(float ratio, int32_t viewRadius, int32_t tickRange);

    // The radius of the chunk view the server keeps around a player (ServerPlayer+3004).
    int32_t computeViewRadius(int32_t chunkRadius, int32_t tickRange);

    bool isNeighbourOffset(int32_t dx, int32_t dz);

    // A ticking area as the decision sees it: its chunk bounds and the width it was declared with.
    struct TickingAreaBounds {
        int32_t mMinChunkX = 0;
        int32_t mMinChunkZ = 0;
        int32_t mMaxChunkX = 0;
        int32_t mMaxChunkZ = 0;
    };

    struct BuildContext {
        int32_t mBuildRadius = MIN_BUILD_RADIUS;
        // Players of the whole level, which sub_14381FD00 reads from Level::getActivePlayerCount.
        size_t mPlayerCount = 0;
        // The chunk of every player in the dimension.
        std::vector<std::pair<int32_t, int32_t>> mPlayerChunks;
        std::vector<TickingAreaBounds> mTickingAreas;
        // Whether storage holds the chunk (DBChunkStorage::isChunkSaved, sub_1406CAF60).
        std::function<bool(int32_t, int32_t)> mIsSaved;
        // Chunks Falcon keeps on the server because its own generation of them is not exact. Not vanilla.
        std::function<bool(int32_t, int32_t)> mIsForced;
    };

    /**
     * Whether the chunk view of a CSG level loads the chunk (sub_1430014E0, generate mode 1). Storage is asked for
     * the chunk first, and for its neighbours only when no player or ticking area keeps it.
     */
    bool isServerBuilt(int32_t chunkX, int32_t chunkZ, const BuildContext &context);

    /**
     * The chunks a client has been sent since its last publisher update (sub_14255D940 adds, sub_14255DB20 drains).
     * The game keeps them in an unordered_set, so a chunk sent twice is listed once.
     */
    class SavedChunkList {
    public:
        void add(int32_t chunkX, int32_t chunkZ);

        bool empty() const {
            return mChunks.empty();
        }

        size_t size() const {
            return mChunks.size();
        }

        std::vector<Vector2i> drain();

        void clear();

    private:
        std::vector<Vector2i> mChunks;
        std::unordered_set<int64_t> mKeys;
    };

    // A map is 128 x 128 pixels, saved as four bytes each in the "colors" array of its map_<id> record.
    constexpr int32_t MAP_PIXEL_COUNT = 128 * 128;

    /**
     * Writes the pixels a client resampled from the chunks it generated into saved map data, as
     * ServerNetworkHandler::handle(MapInfoRequestPacket) does (sub_14228D8A0): a pixel is only taken when it
     * is not blank and differs from the stored one. Returns whether any pixel changed. The game indexes its
     * array without a bound check; indices past the map are dropped here.
     */
    bool applyClientMapPixels(Tag &mapData, const std::vector<MapPixel> &pixels);

    /**
     * Remembers, per structure and region, whether the region's start chunk is a real start, so the parity guard
     * samples the biomes of a candidate only once. Main thread only.
     */
    class StructureGuard {
    public:
        bool isNearStart(const ChunkGenerator &generator, const std::vector<const StructureFeature *> &features,
                         int32_t chunkX, int32_t chunkZ);

        void clear() {
            mStarts.clear();
        }

    private:
        struct Start {
            int32_t mX = 0;
            int32_t mZ = 0;
            bool mValid = false;
        };

        const Start &_start(const ChunkGenerator &generator, const StructureFeature &feature, int32_t regionX,
                            int32_t regionZ);

        std::unordered_map<const StructureFeature *, std::unordered_map<int64_t, Start>> mStarts;
    };
}
