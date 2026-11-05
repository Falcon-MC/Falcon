#include "World/Village/PoiScanner.h"

#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

namespace {
    const int SCAN_RADIUS_XZ = 16;
    const int SCAN_RADIUS_Y = 4;
    // budget checked after the visit and before the cursor moves: 1025 blocks per tick, the last one visited again
    const int VISIT_BUDGET_LIMIT = 1023;
}

const PoiBlueprint *PoiScanner::findValidBlueprint(const BlockState &state) {
    const PoiBlueprint *blueprint = PoiBlueprints::find(state.mName);
    if (blueprint == nullptr)
        return nullptr;

    // only the head piece of an unoccupied bed, as in the game
    if (blueprint->mType == PoiType::Bed
        && (state.mStates.getByte("head_piece_bit", 0) == 0 || state.mStates.getByte("occupied_bit", 0) != 0))
        return nullptr;

    return blueprint;
}

void PoiScanner::visit(Level &level, const KnownPredicate &isKnown,
                       std::vector<std::shared_ptr<PoiInstance>> &found) const {
    const BlockState *state = level.peekBlockPtr(mCursor.x, mCursor.y, mCursor.z);
    if (state == nullptr)
        return;

    const PoiBlueprint *blueprint = findValidBlueprint(*state);
    if (blueprint == nullptr || isKnown(mCursor))
        return;

    found.push_back(std::make_shared<PoiInstance>(mCursor, *blueprint));
}

bool PoiScanner::scan(Level &level, const Vector3i &origin, const KnownPredicate &isKnown,
                      std::vector<std::shared_ptr<PoiInstance>> &found) {
    if (mRestart) {
        mCursor = Vector3i(origin.x - SCAN_RADIUS_XZ, origin.y - SCAN_RADIUS_Y, origin.z - SCAN_RADIUS_XZ);
        mRestart = false;
    }

    // the bounds follow the dweller every tick while the cursor persists: the game lets the box drift the same way
    int visited = 0;
    while (mCursor.x < origin.x + SCAN_RADIUS_XZ) {
        while (mCursor.y < origin.y + SCAN_RADIUS_Y) {
            while (mCursor.z < origin.z + SCAN_RADIUS_XZ) {
                visit(level, isKnown, found);
                if (visited > VISIT_BUDGET_LIMIT)
                    return false;

                ++visited;
                ++mCursor.z;
            }
            mCursor.z = origin.z - SCAN_RADIUS_XZ;
            ++mCursor.y;
        }
        mCursor.y = origin.y - SCAN_RADIUS_Y;
        ++mCursor.x;
    }

    mRestart = true;
    return true;
}
