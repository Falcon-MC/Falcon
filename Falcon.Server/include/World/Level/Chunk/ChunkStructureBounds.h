#pragma once

#include <cstdint>
#include <string>
#include <vector>

// the registry LevelChunk keeps at +0x1600 (ctor sub_143871CB0, 26.60): the structures post processed into the
// chunk, each a name and boxes clipped to the chunk; entity ids number names and boxes together, as entt does
class ChunkStructureBounds {
public:
    struct Box {
        int32_t minX = 0;
        int32_t minY = 0;
        int32_t minZ = 0;
        int32_t maxX = 0;
        int32_t maxY = 0;
        int32_t maxZ = 0;

        bool intersects(const Box &other) const;

        Box clippedTo(const Box &other) const;

        bool contains(int32_t x, int32_t y, int32_t z) const;
    };

    struct Name {
        uint32_t mEntity = 0;
        std::string mName;
    };

    struct Entry {
        uint32_t mEntity = 0;
        uint32_t mName = 0;
        Box mBox;
        // the whole start clipped to the chunk: the position query leaves it out
        bool mWhole = false;
        // the code structures keep the start's vfunc +24 (-3 for swamp huts and outposts); data driven ones do not
        bool mHasStartValue = false;
        int32_t mStartValue = 0;
    };

    static Box chunkBox(int32_t chunkX, int32_t chunkZ, int32_t minY, int32_t maxY);

    bool empty() const {
        return mEntries.empty() && mNames.empty();
    }

    bool hasName(const std::string &name) const;

    // the registration after a chunk post processes a code structure (sub_14387D240 -> sub_14386FE80, 26.60):
    // nothing when the start misses the chunk or a start of that name came first
    void addStart(const std::string &name, int32_t startValue, const Box &start, const std::vector<Box> &pieces,
                  const Box &chunk);

    // Dimension::getGeneratedStructures from the chunk (sub_143871770, 26.60): names of the piece boxes holding the
    // block, newest entry first, each once
    std::vector<std::string> namesAt(int32_t x, int32_t y, int32_t z) const;

    const std::vector<Name> &getNames() const {
        return mNames;
    }

    const std::vector<Entry> &getEntries() const {
        return mEntries;
    }

    // the LevelDB record of tag 'w' (sub_1438724A0, 26.60); empty when nothing is registered
    std::string save() const;

    // false and left empty on a malformed record
    bool load(const std::string &record);

private:
    uint32_t _create();

    std::vector<Name> mNames;
    std::vector<Entry> mEntries;
    uint32_t mNextEntity = 0;
};
