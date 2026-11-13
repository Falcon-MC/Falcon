#include "World/Level/Block/Actor/BlockActor.h"

#include "World/Actor/DynamicPropertyStore.h"
#include "World/Level/Storage/FalconDataVersion.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTable.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"

#include <random>
#include <utility>

namespace {
    const char *TAG_LOOT_TABLE = "LootTable";
    const char *TAG_LOOT_SEED = "LootTableSeed";
    const char *TAG_DYNAMIC_PROPERTIES = "DynamicProperties";
}

Tag BlockActor::getSpawnCompound() const {
    Tag data = Tag::ofCompound();
    data.putString("id", getBlockActorId());
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    return data;
}

Tag BlockActor::saveWithPosition() const {
    Tag data = saveNbt();
    data.putString("id", getBlockActorId());
    data.putLong(FalconDataVersion::TAG, FalconDataVersion::CURRENT);
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    if (!mLootTable.empty()) {
        data.putString(TAG_LOOT_TABLE, mLootTable);
        data.putInt(TAG_LOOT_SEED, mLootSeed);
    }
    if (!mDynamicProperties.empty())
        data.put(TAG_DYNAMIC_PROPERTIES, serializeDynamicProperties(mDynamicProperties));
    if (mEntityStorage.has_value())
        mEntityStorage->save(data);
    return data;
}

// sub_143300BF0: the storage comes back with the tag that holds it
void BlockActor::loadEntityStorage(const Tag &data) {
    if (data.contains("BlockEntityStorage"))
        enableEntityStorage().load(data);
}

void BlockActor::loadDynamicProperties(const Tag &data) {
    mDynamicProperties.clear();
    if (const Tag *properties = data.get(TAG_DYNAMIC_PROPERTIES))
        deserializeDynamicProperties(*properties, mDynamicProperties);
}

void BlockActor::setLootTable(const std::string &table, int32_t seed) {
    mLootTable = table;
    mLootSeed = seed;
}

// sub_144C4D670: both kept as made when the tag has no table
void BlockActor::loadLootTable(const Tag &data) {
    if (!data.contains(TAG_LOOT_TABLE))
        return;
    setLootTable(data.getString(TAG_LOOT_TABLE, std::string()), data.getInt(TAG_LOOT_SEED, 0));
}

void BlockActor::unpackLoot(ServerNetworkHandler &owner) {
    if (mLootTable.empty())
        return;

    const std::string path = std::move(mLootTable);
    mLootTable.clear();

    Container *container = getContainer();
    const LootTable *table = LootTableRegistry::getInstance().get(path);
    if (container == nullptr || table == nullptr)
        return;

    // no player reaches the roll in the game: same seed, same content, whoever opens it
    std::mt19937 random(mLootSeed != 0 ? (uint32_t) mLootSeed : std::random_device{}());
    LootContext context(random);
    LootItems::fillContainer(owner, *container, table->roll(context), random);
}
