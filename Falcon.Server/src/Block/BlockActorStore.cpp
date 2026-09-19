#include "Block/BlockActorStore.h"

#include "Block/Actor/ChestBlockActor.h"
#include "Block/Actor/CommandBlockActor.h"
#include "Block/Actor/ContainerBlockActor.h"
#include "Block/Actor/EnderChestBlockActor.h"
#include "Block/Actor/FurnaceBlockActor.h"
#include "Block/Actor/HopperBlockActor.h"
#include "Block/Actor/ItemFrameBlockActor.h"
#include "Block/Actor/PistonArmBlockActor.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"

#include <utility>

void BlockActorStore::moveStateFrom(BlockActorStore &&other) {
    mBlockActors = std::move(other.mBlockActors);
    mLoadedChunks = std::move(other.mLoadedChunks);

    for (auto &entry: mBlockActors)
        entry.second->setLevel(mLevel);
}

int64_t BlockActorStore::packPosition(const Vector3i &position) {
    return ((int64_t) position.x & 0x3ffffff) << 38 | ((int64_t) position.y & 0xfff) << 26 |
           ((int64_t) position.z & 0x3ffffff);
}

namespace {
    int64_t packChunk(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }
}

BlockActor *BlockActorStore::find(const Vector3i &position) {
    auto it = mBlockActors.find(packPosition(position));
    return it == mBlockActors.end() ? nullptr : it->second.get();
}

void BlockActorStore::insert(std::unique_ptr<BlockActor> blockActor) {
    if (blockActor == nullptr)
        return;

    blockActor->setLevel(mLevel);
    mBlockActors[packPosition(blockActor->getPosition())] = std::move(blockActor);
}

void BlockActorStore::remove(const Vector3i &position) {
    mBlockActors.erase(packPosition(position));
}

std::vector<Tag> BlockActorStore::saveChunk(int32_t chunkX, int32_t chunkZ) const {
    std::vector<Tag> saved;

    for (const auto &entry: mBlockActors) {
        const Vector3i &position = entry.second->getPosition();
        if ((position.x >> 4) != chunkX || (position.z >> 4) != chunkZ)
            continue;

        saved.push_back(entry.second->saveWithPosition());
    }

    return saved;
}

std::string BlockActorStore::encodeChunkNetwork(int32_t chunkX, int32_t chunkZ) const {
    BinaryStream stream;

    for (const auto &entry: mBlockActors) {
        const BlockActor &blockActor = *entry.second;
        const Vector3i &position = blockActor.getPosition();
        if ((position.x >> 4) != chunkX || (position.z >> 4) != chunkZ)
            continue;

        NbtIo::writeTag(stream, blockActor.getSpawnCompound(), NbtVariant::Network);
    }

    return stream.getBuffer();
}

std::string BlockActorStore::encodeSubChunkNetwork(int32_t chunkX, int32_t sectionY, int32_t chunkZ) const {
    BinaryStream stream;

    for (const auto &entry: mBlockActors) {
        const BlockActor &blockActor = *entry.second;
        const Vector3i &position = blockActor.getPosition();
        if ((position.x >> 4) != chunkX || (position.z >> 4) != chunkZ || (position.y >> 4) != sectionY)
            continue;

        NbtIo::writeTag(stream, blockActor.getSpawnCompound(), NbtVariant::Network);
    }

    return stream.getBuffer();
}

std::string BlockActorStore::encodeNetwork(const std::vector<Tag> &blockActors) {
    BinaryStream stream;
    for (const Tag &blockActor: blockActors) {
        if (!blockActor.isCompound())
            continue;

        Tag spawnData = blockActor;
        spawnData.remove("Items");
        NbtIo::writeTag(stream, spawnData, NbtVariant::Network);
    }

    return stream.getBuffer();
}

std::string BlockActorStore::encodeNetworkForSection(const std::vector<Tag> &blockActors, int32_t sectionY) {
    BinaryStream stream;
    for (const Tag &blockActor: blockActors) {
        if (!blockActor.isCompound())
            continue;

        if ((blockActor.getInt("y") >> 4) != sectionY)
            continue;

        Tag spawnData = blockActor;
        spawnData.remove("Items");
        NbtIo::writeTag(stream, spawnData, NbtVariant::Network);
    }

    return stream.getBuffer();
}

bool BlockActorStore::isChunkLoaded(int32_t chunkX, int32_t chunkZ) const {
    return mLoadedChunks.count(packChunk(chunkX, chunkZ)) != 0;
}

void BlockActorStore::unloadChunk(int32_t chunkX, int32_t chunkZ) {
    for (auto it = mBlockActors.begin(); it != mBlockActors.end();) {
        const Vector3i &position = it->second->getPosition();
        if ((position.x >> 4) == chunkX && (position.z >> 4) == chunkZ)
            it = mBlockActors.erase(it);
        else
            ++it;
    }

    mLoadedChunks.erase(packChunk(chunkX, chunkZ));
}

void BlockActorStore::loadChunk(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockActors,
                                const PacketCodecContext &context) {
    if (!mLoadedChunks.insert(packChunk(chunkX, chunkZ)).second)
        return;

    for (const Tag &data: blockActors) {
        if (data.getType() != Tag::Type::Compound)
            continue;

        std::unique_ptr<BlockActor> blockActor = create(data.getString("id"));
        if (blockActor == nullptr)
            continue;

        blockActor->setPosition(Vector3i(data.getInt("x"), data.getInt("y"), data.getInt("z")));
        blockActor->loadNbt(data, context);
        insert(std::move(blockActor));
    }
}

std::unique_ptr<BlockActor> BlockActorStore::create(const std::string &blockActorId) {
    if (blockActorId == ChestBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new ChestBlockActor());
    if (blockActorId == FurnaceBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new FurnaceBlockActor());
    if (blockActorId == PistonArmBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new PistonArmBlockActor());
    if (blockActorId == CommandBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new CommandBlockActor());
    if (blockActorId == BarrelBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new BarrelBlockActor());
    if (blockActorId == ShulkerBoxBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new ShulkerBoxBlockActor());
    if (blockActorId == EnderChestBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new EnderChestBlockActor());
    if (blockActorId == HopperBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new HopperBlockActor());
    if (blockActorId == DispenserBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new DispenserBlockActor());
    if (blockActorId == DropperBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new DropperBlockActor());
    if (blockActorId == BrewingStandBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new BrewingStandBlockActor());
    if (blockActorId == BeaconBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new BeaconBlockActor());
    if (blockActorId == EnchantTableBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new EnchantTableBlockActor());
    if (blockActorId == CrafterBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new CrafterBlockActor());
    if (blockActorId == CampfireBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new CampfireBlockActor());
    if (blockActorId == LecternBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new LecternBlockActor());
    if (blockActorId == ChiseledBookshelfBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new ChiseledBookshelfBlockActor());
    if (blockActorId == DecoratedPotBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new DecoratedPotBlockActor());
    if (blockActorId == JukeboxBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new JukeboxBlockActor());
    if (blockActorId == ShelfBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new ShelfBlockActor());
    if (blockActorId == ItemFrameBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new ItemFrameBlockActor());
    if (blockActorId == GlowItemFrameBlockActor::BLOCK_ACTOR_ID)
        return std::unique_ptr<BlockActor>(new GlowItemFrameBlockActor());

    return nullptr;
}
