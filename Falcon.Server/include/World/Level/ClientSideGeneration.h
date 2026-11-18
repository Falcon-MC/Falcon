#pragma once

#include "Core/Math/Vector3i.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class ChunkGenerator;
class StructureFeature;

/**
 * Client side generation as BDS 1.26.61 runs it, recovered from the Windows client 26.60 which hosts the same
 * server code. With client-side-chunk-generation-enabled, the chunk view a player keeps on the server only loads
 * the chunks the server has to own. The others
 * are never loaded, so they are never sent, and a client that announced CompatibleWithClientSideChunkGen
 * generates them itself. Every chunk the server does send is listed back to the client in the next
 * NetworkChunkPublisherUpdatePacket.
 */
namespace ClientSideGeneration {
    // a player keeps the chunks within this many chunks past its build radius on the server.
    constexpr int32_t PLAYER_MARGIN = 6;

    // the game walks the 13 x 13 chunks around a chunk and keeps the offsets whose squared
    // distance is below this, which leaves out only the four corners.
    constexpr int32_t NEIGHBOUR_RADIUS = 6;
    constexpr float NEIGHBOUR_REACH_SQUARED = 67.76666f;

    // the game never goes below this build radius.
    constexpr int32_t MIN_BUILD_RADIUS = 5;

    // the game caps the server radius it weighs the client against.
    constexpr int32_t MAX_WEIGHED_RADIUS = 28;

    // with a chunk sent since the last update, the next update waits until this long after the
    // last chunk, so it carries every chunk of the burst.
    constexpr int64_t PUBLISHER_DELAY_NANOSECONDS = 251000000;

    // blocks a player moves before its publisher update follows it.
    constexpr int32_t PUBLISHER_MOVE_THRESHOLD = 16;

    // the storage existence cache is emptied once it holds this many chunks.
    constexpr size_t SAVED_CHUNK_CACHE_LIMIT = 30000;

    // How far from the start chunk of a structure the parity guard keeps chunks on the server. Jigsaw structures
    // place pieces up to 80 blocks from their start, and the largest pieces reach a chunk further.
    constexpr int32_t STRUCTURE_GUARD_RADIUS = 7;

    /**
     * The share of the view a CSG client leaves to the server, from
     * the server view distance and the largest radius the client says its hardware handles.
     */
    float computeServerBuildRatio(int32_t serverViewDistance, int32_t clientMaxRadius);

    /**
     * The radius around a player the server builds itself. viewRadius is the radius of the
     * player's chunk view, max(chunk radius, tick distance) + 6.
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
        // Players of the whole level.
        size_t mPlayerCount = 0;
        // The chunk of every player in the dimension.
        std::vector<std::pair<int32_t, int32_t>> mPlayerChunks;
        std::vector<TickingAreaBounds> mTickingAreas;
        // Whether storage holds the chunk.
        std::function<bool(int32_t, int32_t)> mIsSaved;
        // Chunks Falcon keeps on the server because its own generation of them is not exact. Not vanilla.
        std::function<bool(int32_t, int32_t)> mIsForced;
    };

    /**
     * Whether the chunk view of a CSG level loads the chunk. Storage is asked for
     * the chunk first, and for its neighbours only when no player or ticking area keeps it.
     */
    bool isServerBuilt(int32_t chunkX, int32_t chunkZ, const BuildContext &context);

    /**
     * The chunks a client has been sent since its last publisher update.
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
