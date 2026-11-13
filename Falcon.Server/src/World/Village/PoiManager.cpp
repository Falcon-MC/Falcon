#include "World/Village/PoiManager.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Village/PoiBlockTypes.h"

#include <algorithm>

namespace {
    PoiManager::Entry *findEntry(std::vector<PoiManager::Entry> &entries, const Vector3i &position) {
        for (PoiManager::Entry &entry: entries) {
            if (entry.mPosition == position)
                return &entry;
        }
        return nullptr;
    }

    uint8_t ticketsOf(uint16_t type) {
        const PoiBlockType *definition = PoiBlockTypes::get(type);
        return definition == nullptr ? 0 : definition->mTickets;
    }

    int32_t floorDiv16(int32_t value) {
        return value >> 4;
    }
}

PoiManager::ChunkPois *PoiManager::_ensure(Level &level, int32_t chunkX, int32_t chunkZ) {
    LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
    const int64_t key = _key(chunkX, chunkZ);
    if (chunk == nullptr) {
        mChunks.erase(key);
        return nullptr;
    }

    ChunkPois &pois = mChunks[key];
    // another chunk object: the column reloaded and its temporary pois and claimed tickets went with it
    if (!pois.mScanned || pois.mInstance != chunk->getNetworkInstance()) {
        pois = ChunkPois();
        pois.mInstance = chunk->getNetworkInstance();
        pois.mRevision = chunk->getNetworkRevision();
        _scan(level, chunkX, chunkZ, pois);
        pois.mScanned = true;
    } else if (pois.mRevision != chunk->getNetworkRevision()) {
        pois.mRevision = chunk->getNetworkRevision();
        _scan(level, chunkX, chunkZ, pois);
    }
    return &pois;
}

void PoiManager::_scan(Level &level, int32_t chunkX, int32_t chunkZ, ChunkPois &pois) {
    LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
    if (chunk == nullptr)
        return;

    for (Entry &temporary: pois.mTemporary) {
        if (const Entry *current = findEntry(pois.mEntries, temporary.mPosition))
            temporary.mTickets = current->mTickets;
    }

    std::vector<Entry> previous = std::move(pois.mEntries);
    pois.mEntries.clear();
    for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; ++index) {
        const SubChunk &subChunk = static_cast<const LevelChunk &>(*chunk).getSubChunk(index);
        if (subChunk.isEmpty())
            continue;
        const int32_t baseY = LevelChunk::MIN_Y + index * 16;
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                for (int y = 0; y < 16; ++y) {
                    const PoiBlockType *type = PoiBlockTypes::forBlock(subChunk.getBlock(x, y, z).mName);
                    if (type == nullptr)
                        continue;
                    Entry entry;
                    entry.mPosition = Vector3i(chunkX * 16 + x, baseY + y, chunkZ * 16 + z);
                    entry.mType = type->mId;
                    entry.mTickets = type->mTickets;
                    // an unchanged poi keeps the tickets claimed from it
                    const Entry *before = findEntry(previous, entry.mPosition);
                    if (before != nullptr && before->mType == entry.mType)
                        entry.mTickets = before->mTickets;
                    pois.mEntries.push_back(entry);
                }
            }
        }
    }

    for (const Entry &temporary: pois.mTemporary) {
        if (Entry *existing = findEntry(pois.mEntries, temporary.mPosition))
            *existing = temporary;
        else
            pois.mEntries.push_back(temporary);
    }
}

void PoiManager::forEachInBox(Level &level, const Vector3i &min, const Vector3i &max,
                              const std::function<bool(Entry &)> &visitor) {
    for (int32_t chunkX = floorDiv16(min.x); chunkX <= floorDiv16(max.x); ++chunkX) {
        for (int32_t chunkZ = floorDiv16(min.z); chunkZ <= floorDiv16(max.z); ++chunkZ) {
            ChunkPois *pois = _ensure(level, chunkX, chunkZ);
            if (pois == nullptr)
                continue;
            for (Entry &entry: pois->mEntries) {
                const Vector3i &at = entry.mPosition;
                if (at.x < min.x || at.y < min.y || at.z < min.z || at.x > max.x || at.y > max.y || at.z > max.z)
                    continue;
                if (!visitor(entry))
                    return;
            }
        }
    }
}

PoiManager::Entry *PoiManager::at(Level &level, const Vector3i &position) {
    ChunkPois *pois = _ensure(level, floorDiv16(position.x), floorDiv16(position.z));
    return pois == nullptr ? nullptr : findEntry(pois->mEntries, position);
}

void PoiManager::addTemporary(Level &level, const Vector3i &position, uint16_t type) {
    ChunkPois *pois = _ensure(level, floorDiv16(position.x), floorDiv16(position.z));
    if (pois == nullptr)
        return;
    Entry entry;
    entry.mPosition = position;
    entry.mType = type;
    entry.mTickets = ticketsOf(type);
    if (Entry *temporary = findEntry(pois->mTemporary, position))
        *temporary = entry;
    else
        pois->mTemporary.push_back(entry);
    if (Entry *existing = findEntry(pois->mEntries, position))
        *existing = entry;
    else
        pois->mEntries.push_back(entry);
}

bool PoiManager::release(Level &level, const Vector3i &position) {
    Entry *entry = at(level, position);
    if (entry == nullptr)
        return false;
    const PoiBlockType *type = PoiBlockTypes::get(entry->mType);
    if (type == nullptr || entry->mTickets >= type->mTickets)
        return false;
    ++entry->mTickets;
    return true;
}
