#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Village/PoiInstance.h"

#include <functional>
#include <memory>
#include <vector>

class BlockState;
class Level;

// 32x8x32 box around the dweller, walked over several ticks with a fixed block budget per tick
class PoiScanner {
public:
    using KnownPredicate = std::function<bool(const Vector3i &)>;

    // origin: the dweller's floored block position; valid pois isKnown rejects go to found;
    // true once the whole box is walked: the next call starts a new scan
    bool scan(Level &level, const Vector3i &origin, const KnownPredicate &isKnown,
              std::vector<std::shared_ptr<PoiInstance>> &found);

    void restart() {
        mRestart = true;
    }

    // a bed must be a free head piece
    static const PoiBlueprint *findValidBlueprint(const BlockState &state);

private:
    void visit(Level &level, const KnownPredicate &isKnown, std::vector<std::shared_ptr<PoiInstance>> &found) const;

    bool mRestart = true;
    Vector3i mCursor;
};
