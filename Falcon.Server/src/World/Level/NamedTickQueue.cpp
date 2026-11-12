#include "World/Level/NamedTickQueue.h"

#include "Core/NBT/NbtIo.h"
#include "Core/NBT/Tag.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <exception>

namespace {
    const char *const AIR = "minecraft:air";
}

uint32_t NamedTickQueue::hashNamespace(const std::string &name) {
    uint32_t hash = 0x811C9DC5u;
    for (const char character: name)
        hash = (hash ^ (uint8_t) character) * 16777619u;
    return hash;
}

size_t NamedTickQueue::_countAt(const Column &column, const Vector3i &position) {
    size_t count = 0;
    for (const auto &entry: column.entries) {
        if (entry.second.position == position)
            ++count;
    }
    return count;
}

NamedTickQueue::Result NamedTickQueue::schedule(const Vector3i &position, const std::string &blockType,
                                                const std::string &eventNamespace, const std::string &eventName,
                                                int64_t creationTime, int64_t triggerTime) {
    Column &queue = mColumns[column(position.x >> 4, position.z >> 4)];
    if (creationTime < queue.tick)
        return Result::BeforeCurrentTick;
    if (eventNamespace.empty())
        return Result::NoNamespace;
    if (_countAt(queue, position) >= MAX_EVENTS_PER_BLOCK)
        return Result::TooManyEvents;

    const uint32_t hash = hashNamespace(eventNamespace);
    const auto known = queue.namespaces.find(hash);
    if (known != queue.namespaces.end() && known->second != eventNamespace)
        return Result::NamespaceConflict;
    if (known == queue.namespaces.end()) {
        queue.namespaces.emplace(hash, eventNamespace);
        queue.namespaceOrder.push_back(hash);
    }

    // the name lives in a 32-byte buffer: a longer one is left empty
    if (eventName.empty() || eventName.size() > MAX_NAME_LENGTH || triggerTime <= creationTime || blockType.empty()
        || blockType == AIR)
        return Result::InvalidName;

    Entry entry;
    entry.position = position;
    entry.namespaceHash = hash;
    entry.blockType = blockType;
    entry.eventName = eventName;
    entry.creationTime = creationTime;
    entry.triggerTime = triggerTime;
    queue.entries.emplace(triggerTime, std::move(entry));
    return Result::Scheduled;
}

bool NamedTickQueue::has(const Vector3i &position, const std::string &eventNamespace,
                         const std::string &eventName) const {
    const auto found = mColumns.find(column(position.x >> 4, position.z >> 4));
    if (found == mColumns.end())
        return false;

    const uint32_t hash = hashNamespace(eventNamespace);
    for (const auto &entry: found->second.entries) {
        if (entry.second.position == position && entry.second.namespaceHash == hash
            && entry.second.eventName == eventName)
            return true;
    }
    return false;
}

void NamedTickQueue::remove(const Vector3i &position, const std::string &eventNamespace, const std::string &eventName) {
    const auto found = mColumns.find(column(position.x >> 4, position.z >> 4));
    if (found == mColumns.end())
        return;

    const uint32_t hash = hashNamespace(eventNamespace);
    std::multimap<int64_t, Entry> &entries = found->second.entries;
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->second.position == position && it->second.namespaceHash == hash && it->second.eventName == eventName)
            it = entries.erase(it);
        else
            ++it;
    }
}

std::vector<std::pair<int32_t, int32_t>> NamedTickQueue::getColumns() const {
    std::vector<std::pair<int32_t, int32_t>> columns;
    columns.reserve(mColumns.size());
    for (const auto &entry: mColumns)
        columns.emplace_back((int32_t) (entry.first >> 32), (int32_t) (entry.first & 0xffffffff));
    return columns;
}

// sub_1432C7AC0, the event fields by sub_147BBEAC0: position in the chunk, y above its floor
std::string NamedTickQueue::save(int32_t chunkX, int32_t chunkZ, int32_t minY) const {
    const auto found = mColumns.find(column(chunkX, chunkZ));
    if (found == mColumns.end() || found->second.entries.empty())
        return std::string();

    const Column &queue = found->second;
    std::vector<Tag> events;
    for (const auto &item: queue.entries) {
        const Entry &entry = item.second;
        Tag event = Tag::ofCompound();
        event.putByte("x", (int8_t) (entry.position.x & 15));
        event.putShort("y", (int16_t) (uint16_t) (entry.position.y - minY));
        event.putByte("z", (int8_t) (entry.position.z & 15));
        event.putString("blockType", entry.blockType);
        event.putString("eventName", entry.eventName);
        event.putInt("namespaceHash", (int32_t) entry.namespaceHash);
        event.putLong("creationTime", entry.creationTime);
        event.putLong("triggerTime", entry.triggerTime);
        events.push_back(std::move(event));
    }

    std::vector<Tag> namespaces;
    for (const uint32_t hash: queue.namespaceOrder)
        namespaces.push_back(Tag::ofString(queue.namespaces.at(hash)));

    Tag record = Tag::ofCompound();
    record.put("namedTickEventList", Tag::ofList(Tag::Type::Compound, std::move(events)));
    record.put("namedTickEventNamespaceMap", Tag::ofList(Tag::Type::String, std::move(namespaces)));
    record.putLong("currentTick", queue.tick);
    BinaryStream stream;
    NbtIo::writeTag(stream, record, NbtVariant::LittleEndian);
    return stream.getBuffer();
}

// sub_1432C82B0: an event of an unknown namespace, a bad name or time, air or a fifth one at its block is left out
void NamedTickQueue::load(int32_t chunkX, int32_t chunkZ, int32_t minY, const std::string &record, int64_t levelTick) {
    if (record.empty())
        return;

    Tag root;
    try {
        ReadOnlyBinaryStream stream(record);
        root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
    } catch (const std::exception &) {
        return;
    }
    if (!root.isCompound())
        return;

    Column &queue = mColumns[column(chunkX, chunkZ)];
    queue.entries.clear();
    const int64_t savedTick = root.contains("currentTick", Tag::Type::Long) ? root.getLong("currentTick") : 0;
    const int64_t shift = levelTick - (savedTick < 0 ? 0 : savedTick);
    queue.tick = levelTick;

    const Tag *namespaces = root.get("namedTickEventNamespaceMap");
    if (namespaces != nullptr && namespaces->isList()) {
        for (const Tag &name: namespaces->getList()) {
            if (name.getType() != Tag::Type::String || name.asString().empty())
                continue;
            const uint32_t hash = hashNamespace(name.asString());
            if (queue.namespaces.emplace(hash, name.asString()).second)
                queue.namespaceOrder.push_back(hash);
        }
    }

    const Tag *events = root.get("namedTickEventList");
    if (events == nullptr || !events->isList())
        return;
    for (const Tag &event: events->getList()) {
        if (!event.isCompound())
            continue;
        const uint32_t hash = (uint32_t) event.getInt("namespaceHash");
        if (queue.namespaces.find(hash) == queue.namespaces.end())
            continue;

        Entry entry;
        entry.namespaceHash = hash;
        entry.eventName = event.getString("eventName");
        entry.blockType = event.getString("blockType");
        entry.creationTime = event.getLong("creationTime");
        entry.triggerTime = event.getLong("triggerTime");
        if (entry.creationTime < 0 || entry.triggerTime < 0 || entry.blockType.empty() || entry.blockType == AIR
            || entry.eventName.empty() || entry.eventName.size() > MAX_NAME_LENGTH
            || entry.triggerTime <= entry.creationTime)
            continue;

        entry.position = Vector3i(chunkX * 16 + (uint8_t) event.getByte("x"), minY + (uint16_t) event.getShort("y"),
                                  chunkZ * 16 + (uint8_t) event.getByte("z"));
        if (_countAt(queue, entry.position) >= MAX_EVENTS_PER_BLOCK)
            continue;
        entry.creationTime += shift;
        entry.triggerTime += shift;
        const int64_t trigger = entry.triggerTime;
        queue.entries.emplace(trigger, std::move(entry));
    }
}
