#include "World/Level/Map/MapRequestQueue.h"

#include <algorithm>

bool MapRequestQueue::push(int64_t mapId) {
    if (mIds.size() >= CAPACITY || std::find(mIds.begin(), mIds.end(), mapId) != mIds.end())
        return false;

    mIds.push_back(mapId);
    return true;
}

std::vector<int64_t> MapRequestQueue::takeDue() {
    const size_t count = std::min(mIds.size(), PER_TICK);
    std::vector<int64_t> due(mIds.begin(), mIds.begin() + (std::ptrdiff_t) count);
    mIds.erase(mIds.begin(), mIds.begin() + (std::ptrdiff_t) count);
    return due;
}
