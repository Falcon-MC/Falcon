#pragma once

#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"
#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

#include <vector>

class MangroveTree : public VanillaTreeObject {
public:
    MangroveTree();

    explicit MangroveTree(bool tall);

    void setWithBeeNest(bool withBeeNest) { mWithBeeNest = withBeeNest; }

    void setBeeCount(int32_t beeCount) { mBeeCount = beeCount; }

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    struct Coordinate {
        int32_t mX;
        int32_t mY;
        int32_t mZ;

        bool operator==(const Coordinate &other) const {
            return mX == other.mX && mY == other.mY && mZ == other.mZ;
        }
    };

    struct MangroveProperties {
        int32_t mBaseHeight;
        int32_t mHeightRandA;
        int32_t mHeightRandB;
        int32_t mExtraBranchStepsMin;
        int32_t mExtraBranchStepsMax;
        int32_t mExtraBranchLengthMin;
        int32_t mExtraBranchLengthMax;
        int32_t mRootOffsetMin;
        int32_t mRootOffsetMax;
    };

    std::vector<Coordinate> _placeTrunk(BlockManager &manager, IRandom &random, int32_t treeHeight,
                                        const Coordinate &origin, const MangroveProperties &properties);

    void _placeBranch(BlockManager &manager, int32_t treeHeight, std::vector<Coordinate> &attachments,
                      const Coordinate &logPos, int32_t currentHeight, TreeBlockFace branchDir, int32_t branchPos,
                      int32_t branchSteps);

    bool _placeRoots(BlockManager &manager, IRandom &random, const Coordinate &origin, const Coordinate &trunkOrigin);

    bool _simulateRoots(BlockManager &manager, IRandom &random, const Coordinate &rootPos, TreeBlockFace dir,
                        const Coordinate &rootOrigin, std::vector<Coordinate> &rootPositions, int32_t layer);

    std::vector<Coordinate> _potentialRootPositions(const Coordinate &pos, TreeBlockFace previousDir, IRandom &random,
                                                    const Coordinate &rootOrigin);

    void _placeRoot(BlockManager &manager, IRandom &random, const Coordinate &pos);

    void _createRandomSpreadFoliage(BlockManager &manager, IRandom &random, const Coordinate &origin);

    void _placeLeafVines(BlockManager &manager, IRandom &random, const std::vector<Coordinate> &foliageAttachments);

    void _maybePlaceVine(BlockManager &manager, IRandom &random, const Coordinate &pos, TreeBlockFace attachedTo);

    void _addHangingVine(BlockManager &manager, const Coordinate &pos, TreeBlockFace attachedTo);

    void _placePropagules(BlockManager &manager, IRandom &random, const std::vector<Coordinate> &foliageAttachments);

    bool _hasRequiredEmptyBlocks(BlockManager &manager, const Coordinate &leafPos, TreeBlockFace direction,
                                 int32_t count);

    void _placeBeeNest(BlockManager &manager, IRandom &random, const std::vector<Coordinate> &foliageAttachments);

    std::vector<Coordinate> _collectPlacedLeaves(BlockManager &manager,
                                                 const std::vector<Coordinate> &foliageAttachments);

    bool _placeLog(BlockManager &manager, const Coordinate &pos);

    void _placeLeaf(BlockManager &manager, const Coordinate &pos);

    void _placeVine(BlockManager &manager, const Coordinate &pos, TreeBlockFace attachedTo);

    bool mWithBeeNest = false;
    int32_t mBeeCount = 3;
    bool mTall;
};
