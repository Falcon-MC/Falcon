#pragma once

#include "Core/Math/Vector3i.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// the script named ticks of each chunk;
// times are level ticks, saved under LevelDB tag 'z'
class NamedTickQueue {
public:
    struct Entry {
        Vector3i position;
        uint32_t namespaceHash = 0;
        std::string blockType;
        std::string eventName;
        int64_t creationTime = 0;
        int64_t triggerTime = 0;
    };

    // the codes the game returns, in its order
    enum class Result { Scheduled, BeforeCurrentTick, NoNamespace, TooManyEvents, NamespaceConflict, InvalidName };

    static constexpr size_t MAX_EVENTS_PER_BLOCK = 4;
    static constexpr size_t MAX_NAME_LENGTH = 31;
    static constexpr size_t MAX_EVENTS_PER_TICK = 100;

    // 32-bit FNV-1a of the namespace string
    static uint32_t hashNamespace(const std::string &name);

    // `blockType` empty or air: the game refuses an event on air
    Result schedule(const Vector3i &position, const std::string &blockType, const std::string &eventNamespace,
                    const std::string &eventName, int64_t creationTime, int64_t triggerTime);

    bool has(const Vector3i &position, const std::string &eventNamespace, const std::string &eventName) const;

    void remove(const Vector3i &position, const std::string &eventNamespace, const std::string &eventName);

    // one chunk's turn: once per level tick, at most 100 due events in trigger order; the entry leaves the queue
    // before `handler(entry, eventNamespace)` runs
    template <typename Handler>
    void tickColumn(int32_t chunkX, int32_t chunkZ, int64_t levelTick, Handler &&handler);

    // the chunk's record, empty when no event waits
    std::string save(int32_t chunkX, int32_t chunkZ, int32_t minY) const;

    // a saved currentTick becomes `levelTick`: the level clock restarts with the server
    void load(int32_t chunkX, int32_t chunkZ, int32_t minY, const std::string &record, int64_t levelTick);

    void drop(int32_t chunkX, int32_t chunkZ) {
        mColumns.erase(column(chunkX, chunkZ));
    }

    bool hasColumns() const {
        return !mColumns.empty();
    }

    std::vector<std::pair<int32_t, int32_t>> getColumns() const;

private:
    struct Column {
        int64_t tick = 0;
        // ordered by trigger time; equal times keep their order
        std::multimap<int64_t, Entry> entries;
        std::unordered_map<uint32_t, std::string> namespaces;
        std::vector<uint32_t> namespaceOrder;
    };

    static int64_t column(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }

    static size_t _countAt(const Column &column, const Vector3i &position);

    std::unordered_map<int64_t, Column> mColumns;
};

template <typename Handler>
void NamedTickQueue::tickColumn(int32_t chunkX, int32_t chunkZ, int64_t levelTick, Handler &&handler) {
    auto found = mColumns.find(column(chunkX, chunkZ));
    if (found == mColumns.end() || found->second.tick == levelTick)
        return;

    found->second.tick = levelTick;
    for (size_t processed = 0; processed < MAX_EVENTS_PER_TICK; ++processed) {
        // the handler may schedule into this chunk: look the column up again
        found = mColumns.find(column(chunkX, chunkZ));
        if (found == mColumns.end() || found->second.entries.empty()
            || found->second.entries.begin()->first > levelTick)
            return;

        Column &queue = found->second;
        const Entry entry = queue.entries.begin()->second;
        queue.entries.erase(queue.entries.begin());
        const auto name = queue.namespaces.find(entry.namespaceHash);
        if (name == queue.namespaces.end())
            continue;
        const std::string eventNamespace = name->second;
        handler(entry, eventNamespace);
    }
}
