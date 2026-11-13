#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class StructureTemplate;

/**
 * The jigsaw assembly of the data driven structures (worldgen/jigsaw_structures). The start piece is laid first, then
 * pieces are grown breadth first from a queue ordered by the jigsaws' placement priority, each one kept inside the
 * free volume of its parent's branch. Villages use their own assembly (JigsawAssembler): the game grows them in
 * another order with another random.
 */
class JigsawPlacer {
public:
    // 0 down, 1 up, 2 north, 3 south, 4 west, 5 east
    using Facing = int32_t;

    struct Jigsaw {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        Facing mFront = 0;
        // compared for an aligned joint on a vertical front only
        Facing mTop = 1;
        std::string mName;
        std::string mTarget;
        std::string mPool;
        std::string mFinalState;
        bool mAligned = false;
        int32_t mSelectionPriority = 0;
        int32_t mPlacementPriority = 0;
    };

    // jigsaws in the order of the template's block list
    struct Template {
        int32_t mSizeX = 0;
        int32_t mSizeY = 0;
        int32_t mSizeZ = 0;
        std::vector<Jigsaw> mJigsaws;
    };

    struct Element {
        std::string mLocation;
        std::string mProcessors;
        bool mTerrainMatching = false;
    };

    // one slot per weight unit; a null slot is the empty element
    struct Pool {
        std::vector<const Element *> mSlots;
        std::string mFallback;
    };

    struct Box {
        int32_t mMinX = 0;
        int32_t mMinY = 0;
        int32_t mMinZ = 0;
        int32_t mMaxX = 0;
        int32_t mMaxY = 0;
        int32_t mMaxZ = 0;
    };

    // where a piece meets its neighbour, for the terrain adaptation
    struct Junction {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        int32_t mDeltaY = 0;
        bool mTerrainMatching = false;
    };

    struct Piece {
        const Element *mElement = nullptr;
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        int32_t mQuarterTurns = 0;
        int32_t mGroundLevelDelta = 1;
        int32_t mDepth = 0;
        Box mBox;
        std::vector<Junction> mJunctions;
    };

    // heightmap_projection: world_surface or ocean_floor of the noise column
    enum class Projection : uint8_t { None, WorldSurface, OceanFloor };

    struct Settings {
        std::string mStartPool;
        std::string mStartJigsaw;
        int32_t mMaxDepth = 0;
        int32_t mStartX = 0;
        int32_t mStartY = 0;
        int32_t mStartZ = 0;
        Projection mProjection = Projection::None;
        int32_t mMaxDistanceHorizontal = 0;
        int32_t mMaxDistanceVertical = 0;
        int32_t mPaddingTop = 0;
        int32_t mPaddingBottom = 0;
        int32_t mDimensionMinY = 0;
        // the game bounds the top with the dimension's height, not with its top y
        int32_t mDimensionHeight = 0;
    };

    struct Result {
        // the start position: the start piece's centre in x and z
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        // empty for a max depth of 0, which the game places nothing for
        std::vector<Piece> mPieces;
    };

    // exclusive bound
    using NextInt = std::function<int32_t(int32_t bound)>;
    // null when the game has no pool of that name
    using PoolLookup = std::function<const Pool *(const std::string &name)>;
    using TemplateLookup = std::function<std::shared_ptr<const Template>(const Element &element)>;
    using Height = std::function<int32_t(int32_t x, int32_t z, Projection projection)>;

    JigsawPlacer(NextInt nextInt, PoolLookup pools, TemplateLookup templates, Height height);

    // false when the start pool, its drawn element or the start jigsaw is missing, or the padding rejects the start
    bool place(const Settings &settings, Result &result);

    // the jigsaws of a game template: front, top and data of each jigsaw block
    static std::shared_ptr<const Template> templateOf(const StructureTemplate &source);

    static Facing turn(Facing facing, int32_t quarterTurns);

    static bool canAttach(const Jigsaw &parent, int32_t parentTurns, const Jigsaw &child, int32_t childTurns);

private:
    struct Shape {
        Box mBounds;
        std::vector<Box> mTaken;
    };

    struct Queued {
        int32_t mPriority = 0;
        int32_t mSequence = 0;
        size_t mPiece = 0;
        size_t mShape = 0;
        int32_t mDepth = 0;
    };

    static bool _later(const Queued &left, const Queued &right);

    std::vector<size_t> _shuffledJigsaws(const Template &source);

    template <typename T>
    void _shuffle(std::vector<T> &values);

    void _expand(const Queued &entry);

    bool _poolUsable(const std::string &name, const Pool *&pool);

    NextInt mNextInt;
    PoolLookup mPools;
    TemplateLookup mTemplates;
    Height mHeight;
    int32_t mMaxDepth = 0;
    std::vector<Piece> mPieces;
    std::vector<Shape> mShapes;
    std::vector<Queued> mQueue;
    int32_t mSequence = 0;
};
