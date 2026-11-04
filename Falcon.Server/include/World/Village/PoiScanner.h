#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Village/PoiInstance.h"

#include <functional>
#include <memory>
#include <vector>

class BlockState;
class Level;

/**
 * The block scan a dweller asks the village manager for: a 32x8x32 box around the dweller, walked over several
 * ticks with a fixed block budget per tick.
 */
class PoiScanner {
public:
    using KnownPredicate = std::function<bool(const Vector3i &)>;

    /**
     * Continues the scan around origin, the dweller's floored block position. Blocks holding a valid point of
     * interest that isKnown rejects are appended to found. Returns true once the whole box has been walked; the
     * next call then starts a new scan.
     */
    bool scan(Level &level, const Vector3i &origin, const KnownPredicate &isKnown,
              std::vector<std::shared_ptr<PoiInstance>> &found);

    /** Makes the next scan start over from the corner of a new box. */
    void restart() { mRestart = true; }

    /** The blueprint of a block that can become a point of interest: a bed must be a free head piece. */
    static const PoiBlueprint *findValidBlueprint(const BlockState &state);

private:
    void visit(Level &level, const KnownPredicate &isKnown, std::vector<std::shared_ptr<PoiInstance>> &found) const;

    bool mRestart = true;
    Vector3i mCursor;
};
