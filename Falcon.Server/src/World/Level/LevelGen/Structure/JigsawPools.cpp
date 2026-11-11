#include "World/Level/LevelGen/Structure/JigsawPools.h"

const JigsawPools::Pool *JigsawPools::get(const std::string &name) {
    const auto found = _pools().find(name);
    return found == _pools().end() ? nullptr : &found->second;
}

const std::unordered_map<std::string, JigsawPools::Pool> &JigsawPools::_pools() {
    static const std::unordered_map<std::string, Pool> pools = []() {
        std::unordered_map<std::string, const JigsawElementData *> elements;
        for (const JigsawElementData &element: JigsawPoolData::ELEMENTS)
            elements[element.mName] = &element;

        std::unordered_map<std::string, Pool> result;
        for (const JigsawPoolEntryData &entry: JigsawPoolData::POOLS) {
            Pool &pool = result[entry.mName];
            pool.mFallback = entry.mFallback;
            for (const JigsawPoolItemData &item: entry.mItems) {
                const auto element = elements.find(item.mElement);
                const JigsawElementData *slot = element == elements.end() ? nullptr : element->second;
                pool.mSlots.insert(pool.mSlots.end(), (size_t) item.mWeight, slot);
            }
        }

        return result;
    }();

    return pools;
}
