#include "World/Level/Chunk/ChunkStructureBounds.h"

#include <algorithm>
#include <initializer_list>
#include <unordered_map>

namespace {
    const int32_t RECORD_VERSION = 1;
    const uint32_t MAX_STRING_LENGTH = 0x7fff;

    void appendInt(std::string &out, int32_t value) {
        const uint32_t bits = (uint32_t) value;
        out.push_back((char) (bits & 0xff));
        out.push_back((char) ((bits >> 8) & 0xff));
        out.push_back((char) ((bits >> 16) & 0xff));
        out.push_back((char) ((bits >> 24) & 0xff));
    }

    class Reader {
    public:
        explicit Reader(const std::string &data) : mData(data) {}

        bool readInt(int32_t &value) {
            if (mData.size() - mOffset < 4)
                return false;
            uint32_t bits = 0;
            for (int i = 3; i >= 0; --i)
                bits = bits << 8 | (unsigned char) mData[mOffset + (size_t) i];
            mOffset += 4;
            value = (int32_t) bits;
            return true;
        }

        bool readCount(size_t &count) {
            int32_t value = 0;
            if (!readInt(value) || value < 0)
                return false;
            count = (size_t) value;
            return true;
        }

        bool readString(std::string &value) {
            if (mData.size() - mOffset < 2)
                return false;
            const size_t length = (size_t) ((unsigned char) mData[mOffset] | ((unsigned char) mData[mOffset + 1] << 8));
            mOffset += 2;
            if (mData.size() - mOffset < length)
                return false;
            value.assign(mData, mOffset, length);
            mOffset += length;
            return true;
        }

    private:
        const std::string &mData;
        size_t mOffset = 0;
    };
}

bool ChunkStructureBounds::Box::intersects(const Box &other) const {
    return maxX >= other.minX && minX <= other.maxX && maxZ >= other.minZ && minZ <= other.maxZ && maxY >= other.minY
           && minY <= other.maxY;
}

ChunkStructureBounds::Box ChunkStructureBounds::Box::clippedTo(const Box &other) const {
    Box box;
    box.minX = std::max(minX, other.minX);
    box.minY = std::max(minY, other.minY);
    box.minZ = std::max(minZ, other.minZ);
    box.maxX = std::min(maxX, other.maxX);
    box.maxY = std::min(maxY, other.maxY);
    box.maxZ = std::min(maxZ, other.maxZ);
    return box;
}

bool ChunkStructureBounds::Box::contains(int32_t x, int32_t y, int32_t z) const {
    return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
}

// the box LevelChunk keeps at +0x1900: the dimension's height, top excluded
ChunkStructureBounds::Box ChunkStructureBounds::chunkBox(int32_t chunkX, int32_t chunkZ, int32_t minY, int32_t maxY) {
    Box box;
    box.minX = chunkX * 16;
    box.minY = minY;
    box.minZ = chunkZ * 16;
    box.maxX = chunkX * 16 + 15;
    box.maxY = maxY;
    box.maxZ = chunkZ * 16 + 15;
    return box;
}

bool ChunkStructureBounds::hasName(const std::string &name) const {
    for (const Name &entry: mNames) {
        if (entry.mName == name)
            return true;
    }
    return false;
}

uint32_t ChunkStructureBounds::_create() {
    return mNextEntity++;
}

void ChunkStructureBounds::addStart(const std::string &name, int32_t startValue, const Box &start,
                                    const std::vector<Box> &pieces, const Box &chunk) {
    // the game tests the start box first; the name entity comes before any box
    if (!start.intersects(chunk) || hasName(name))
        return;

    const uint32_t nameEntity = _create();
    mNames.push_back({nameEntity, name});

    Entry whole;
    whole.mEntity = _create();
    whole.mName = nameEntity;
    whole.mBox = start.clippedTo(chunk);
    whole.mWhole = true;
    whole.mHasStartValue = true;
    whole.mStartValue = startValue;
    mEntries.push_back(whole);

    for (const Box &piece: pieces) {
        if (!piece.intersects(chunk))
            continue;

        Entry entry;
        entry.mEntity = _create();
        entry.mName = nameEntity;
        entry.mBox = piece.clippedTo(chunk);
        entry.mHasStartValue = true;
        entry.mStartValue = startValue;
        mEntries.push_back(entry);
    }
}

std::vector<std::string> ChunkStructureBounds::namesAt(int32_t x, int32_t y, int32_t z) const {
    std::vector<std::string> names;
    for (auto entry = mEntries.rbegin(); entry != mEntries.rend(); ++entry) {
        if (entry->mWhole || !entry->mBox.contains(x, y, z))
            continue;

        for (const Name &name: mNames) {
            if (name.mEntity != entry->mName)
                continue;
            if (std::find(names.begin(), names.end(), name.mName) == names.end())
                names.push_back(name.mName);
            break;
        }
    }
    return names;
}

// version 1, then every section in reverse insertion order: names (entity, string), boxes (entity, 6 ints), data
// driven entries (entity, name, whole), code ones (entity, name, start value, whole); all ints little endian
std::string ChunkStructureBounds::save() const {
    std::string out;
    if (mEntries.empty())
        return out;

    appendInt(out, RECORD_VERSION);
    appendInt(out, (int32_t) mNames.size());
    for (auto name = mNames.rbegin(); name != mNames.rend(); ++name) {
        appendInt(out, (int32_t) name->mEntity);
        const size_t length = std::min<size_t>(name->mName.size(), MAX_STRING_LENGTH);
        out.push_back((char) (length & 0xff));
        out.push_back((char) ((length >> 8) & 0xff));
        out.append(name->mName, 0, length);
    }

    appendInt(out, (int32_t) mEntries.size());
    for (auto entry = mEntries.rbegin(); entry != mEntries.rend(); ++entry) {
        appendInt(out, (int32_t) entry->mEntity);
        appendInt(out, entry->mBox.minX);
        appendInt(out, entry->mBox.minY);
        appendInt(out, entry->mBox.minZ);
        appendInt(out, entry->mBox.maxX);
        appendInt(out, entry->mBox.maxY);
        appendInt(out, entry->mBox.maxZ);
    }

    for (const bool code: {false, true}) {
        int32_t count = 0;
        for (const Entry &entry: mEntries)
            count += entry.mHasStartValue == code ? 1 : 0;
        appendInt(out, count);
        for (auto entry = mEntries.rbegin(); entry != mEntries.rend(); ++entry) {
            if (entry->mHasStartValue != code)
                continue;
            appendInt(out, (int32_t) entry->mEntity);
            appendInt(out, (int32_t) entry->mName);
            if (code)
                appendInt(out, entry->mStartValue);
            appendInt(out, entry->mWhole ? 1 : 0);
        }
    }
    return out;
}

bool ChunkStructureBounds::load(const std::string &record) {
    mNames.clear();
    mEntries.clear();
    mNextEntity = 0;

    Reader reader(record);
    int32_t version = 0;
    if (!reader.readInt(version) || version != RECORD_VERSION)
        return false;

    std::vector<Name> names;
    std::vector<Entry> entries;
    std::unordered_map<uint32_t, size_t> byEntity;
    bool valid = true;
    size_t count = 0;
    uint32_t highest = 0;
    bool any = false;
    const auto seen = [&](uint32_t entity) {
        highest = any ? std::max(highest, entity) : entity;
        any = true;
    };

    valid = reader.readCount(count);
    for (size_t i = 0; valid && i < count; ++i) {
        int32_t entity = 0;
        Name name;
        valid = reader.readInt(entity) && reader.readString(name.mName);
        name.mEntity = (uint32_t) entity;
        seen(name.mEntity);
        names.push_back(name);
    }

    valid = valid && reader.readCount(count);
    for (size_t i = 0; valid && i < count; ++i) {
        int32_t entity = 0;
        Entry entry;
        valid = reader.readInt(entity) && reader.readInt(entry.mBox.minX) && reader.readInt(entry.mBox.minY)
                && reader.readInt(entry.mBox.minZ) && reader.readInt(entry.mBox.maxX) && reader.readInt(entry.mBox.maxY)
                && reader.readInt(entry.mBox.maxZ);
        entry.mEntity = (uint32_t) entity;
        seen(entry.mEntity);
        byEntity[entry.mEntity] = entries.size();
        entries.push_back(entry);
    }

    std::vector<bool> named(entries.size(), false);
    for (const bool code: {false, true}) {
        valid = valid && reader.readCount(count);
        for (size_t i = 0; valid && i < count; ++i) {
            int32_t entity = 0;
            int32_t name = 0;
            int32_t value = 0;
            int32_t whole = 0;
            valid = reader.readInt(entity) && reader.readInt(name) && (!code || reader.readInt(value))
                    && reader.readInt(whole);
            const auto found = byEntity.find((uint32_t) entity);
            if (!valid || found == byEntity.end())
                continue;
            Entry &entry = entries[found->second];
            entry.mName = (uint32_t) name;
            entry.mWhole = whole != 0;
            entry.mHasStartValue = code;
            entry.mStartValue = value;
            named[found->second] = true;
        }
    }
    if (!valid)
        return false;

    // written newest first
    mNames.assign(names.rbegin(), names.rend());
    for (size_t i = entries.size(); i-- > 0;) {
        if (named[i])
            mEntries.push_back(entries[i]);
    }
    mNextEntity = any ? highest + 1 : 0;
    return true;
}
