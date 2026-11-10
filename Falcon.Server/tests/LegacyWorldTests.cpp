#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Storage/LegacyWorldImporter.h"
#include "World/Level/Storage/LevelStorage.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
    int gFailures = 0;

    void check(bool condition, const std::string &name) {
        if (!condition) {
            gFailures++;
            std::printf("FAIL: %s\n", name.c_str());
        }
    }

    void putLInt(std::string &out, uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back((char) ((value >> shift) & 0xff));
    }

    void write(const std::filesystem::path &path, const std::string &bytes) {
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), (std::streamsize) bytes.size());
    }

    // one block of a chunk in the layout of chunks.dat: x, then z, then y fastest
    void setLegacy(std::string &ids, std::string &metas, int x, int y, int z, uint8_t id, uint8_t meta) {
        const size_t index = ((size_t) x * 16 + (size_t) z) * 128 + (size_t) y;
        ids[index] = (char) id;
        uint8_t packed = (uint8_t) metas[index >> 1];
        packed = (index & 1) == 0 ? (uint8_t) ((packed & 0xf0) | meta) : (uint8_t) ((packed & 0x0f) | (meta << 4));
        metas[index >> 1] = (char) packed;
    }

    Tag floats(float x, float y, float z) {
        Tag list = Tag::ofList(Tag::Type::Float);
        list.addToList(Tag::ofFloat(x));
        list.addToList(Tag::ofFloat(y));
        list.addToList(Tag::ofFloat(z));
        return list;
    }

    Tag oldItem(int16_t id, int8_t count, int16_t damage) {
        Tag item = Tag::ofCompound();
        item.putShort("id", id);
        item.putByte("Count", count);
        item.putShort("Damage", damage);
        return item;
    }

    // entities.dat: "ENT", a version, the length, then the compound
    void writeEntities(const std::filesystem::path &root) {
        Tag chicken = Tag::ofCompound();
        chicken.putInt("id", 10);
        chicken.put("Pos", floats(40.5f, 70.0f, 50.5f));
        Tag drop = Tag::ofCompound();
        drop.putInt("id", 64);
        drop.put("Pos", floats(-3.5f, 64.0f, 20.0f));
        drop.put("Item", oldItem(280, 3, 0));
        Tag unknown = Tag::ofCompound();
        unknown.putInt("id", 250);
        unknown.put("Pos", floats(1.0f, 1.0f, 1.0f));
        Tag entities = Tag::ofList(Tag::Type::Compound);
        entities.addToList(chicken);
        entities.addToList(drop);
        entities.addToList(unknown);

        Tag chest = Tag::ofCompound();
        chest.putString("id", "Chest");
        chest.putInt("x", 40);
        chest.putInt("y", 70);
        chest.putInt("z", 51);
        Tag diamonds = oldItem(264, 5, 0);
        diamonds.putByte("Slot", 0);
        Tag wool = oldItem(35, 1, 14);
        wool.putByte("Slot", 1);
        Tag items = Tag::ofList(Tag::Type::Compound);
        items.addToList(diamonds);
        items.addToList(wool);
        chest.put("Items", items);
        Tag sign = Tag::ofCompound();
        sign.putString("id", "Sign");
        sign.putInt("x", 41);
        sign.putInt("y", 70);
        sign.putInt("z", 51);
        sign.putString("Text1", "hello");
        sign.putString("Text2", "world");
        sign.putString("Text3", "");
        sign.putString("Text4", "");
        Tag tiles = Tag::ofList(Tag::Type::Compound);
        tiles.addToList(chest);
        tiles.addToList(sign);

        Tag data = Tag::ofCompound();
        data.put("Entities", entities);
        data.put("TileEntities", tiles);
        BinaryStream body;
        NbtIo::writeTag(body, data, NbtVariant::LittleEndian);
        std::string file("ENT", 4);
        putLInt(file, 1);
        putLInt(file, (uint32_t) body.getBuffer().size());
        write(root / "entities.dat", file + body.getBuffer());
    }

    // a world as 0.8 saved it: level.dat with its 8 byte header, chunks.dat with one chunk at 2 3
    void writeWorld(const std::filesystem::path &root) {
        std::filesystem::create_directories(root);

        Tag data = Tag::ofCompound();
        data.putString("LevelName", "legacy");
        data.putLong("RandomSeed", 1234);
        data.putInt("SpawnX", 40);
        data.putInt("SpawnY", 70);
        data.putInt("SpawnZ", 50);
        data.putInt("StorageVersion", 3);
        BinaryStream body;
        NbtIo::writeTag(body, data, NbtVariant::LittleEndian);
        std::string levelDat;
        putLInt(levelDat, 3);
        putLInt(levelDat, (uint32_t) body.getBuffer().size());
        write(root / "level.dat", levelDat + body.getBuffer());

        std::string ids(32768, '\0');
        std::string metas(16384, '\0');
        setLegacy(ids, metas, 1, 2, 3, 1, 0);
        setLegacy(ids, metas, 4, 5, 6, 35, 14);
        setLegacy(ids, metas, 7, 8, 9, 5, 1);
        setLegacy(ids, metas, 0, 0, 0, 7, 0);
        // storage version 3 keeps sky and block light after the metas
        const std::string payload = ids + metas + std::string(16384, '\0') + std::string(16384, '\0');

        std::string chunk;
        putLInt(chunk, (uint32_t) payload.size() + 4);
        chunk += payload;
        const size_t sectors = (chunk.size() + 4095) / 4096;
        chunk.resize(sectors * 4096, '\0');

        std::string file(4096, '\0');
        const uint32_t entry = (1u << 8) | (uint32_t) sectors;
        const size_t slot = (2 + 3 * 32) * 4;
        for (int shift = 0; shift < 32; shift += 8)
            file[slot + (size_t) (shift / 8)] = (char) ((entry >> shift) & 0xff);
        write(root / "chunks.dat", file + chunk);

        writeEntities(root);
    }
}

int main() {
    const std::filesystem::path worlds = std::filesystem::temp_directory_path() / "falcon-legacy-world-test";
    std::filesystem::remove_all(worlds);
    writeWorld(worlds / "legacy");
    const auto chunksBefore = std::filesystem::file_size(worlds / "legacy" / "chunks.dat");

    check(LegacyWorldImporter::isLegacyWorld(worlds.string(), "legacy"), "an old world is recognised");
    check(LegacyWorldImporter::importIfNeeded(worlds.string(), "legacy"), "the world is imported");
    check(!LegacyWorldImporter::importIfNeeded(worlds.string(), "legacy"), "a world is imported only once");
    check(std::filesystem::file_size(worlds / "legacy" / "chunks.dat") == chunksBefore, "chunks.dat is left as it was");

    LevelStorage storage;
    check(storage.open(worlds.string(), "legacy", 0), "the imported world opens");
    LevelChunk chunk(2, 3);
    check(storage.loadChunk(chunk), "the chunk was written");
    check(chunk.isPopulated(), "the chunk is not decorated again");
    check(chunk.getBlock(1, 2, 3).mName == "minecraft:stone", "stone");
    check(chunk.getBlock(4, 5, 6).mName == "minecraft:red_wool", "wool keeps its colour");
    check(chunk.getBlock(7, 8, 9).mName == "minecraft:spruce_planks", "planks keep their wood");
    check(chunk.getBlock(0, 0, 0).mName == "minecraft:bedrock", "the lowest block keeps its height");
    check(chunk.getBlock(1, 3, 3).mName == "minecraft:air", "air stays air");
    LevelChunk missing(0, 0);
    check(!storage.loadChunk(missing), "a chunk never saved stays absent");

    const std::vector<Tag> mobs = storage.loadEntities(2, 3);
    check(mobs.size() == 1 && mobs[0].getString("identifier") == "minecraft:chicken", "a chicken keeps its kind");
    const std::vector<Tag> drops = storage.loadEntities(-1, 1);
    check(drops.size() == 1 && drops[0].getString("identifier") == "minecraft:item", "a dropped item stays one");
    const Tag *dropped = drops.empty() ? nullptr : drops[0].get("Item");
    check(dropped != nullptr && dropped->getString("Name") == "minecraft:stick" && dropped->getByte("Count") == 3,
          "the dropped item is named");
    check(storage.loadEntities(0, 0).empty(), "an unknown entity is left out");

    const std::vector<Tag> tiles = storage.loadBlockEntities(2, 3);
    check(tiles.size() == 2, "both block entities are kept");
    for (const Tag &tile: tiles) {
        if (tile.getString("id") == "Chest") {
            const Tag *items = tile.get("Items");
            check(items != nullptr && items->getList().size() == 2, "the chest keeps its items");
            if (items != nullptr && items->getList().size() == 2) {
                check(items->getList()[0].getString("Name") == "minecraft:diamond", "diamonds are named");
                check(!items->getList()[0].contains("id"), "the numeric id is gone");
                check(items->getList()[1].getString("Name") == "minecraft:wool"
                      && items->getList()[1].getShort("Damage") == 14, "wool keeps its colour as damage");
            }
        } else {
            const Tag *front = tile.get("FrontText");
            check(front != nullptr && front->getString("Text") == "hello\nworld", "the sign keeps its lines");
            check(!tile.contains("Text1"), "the old lines are gone");
        }
    }
    storage.close();

    std::filesystem::remove_all(worlds);
    if (gFailures == 0)
        std::printf("Legacy world checks passed\n");
    return gFailures == 0 ? 0 : 1;
}
