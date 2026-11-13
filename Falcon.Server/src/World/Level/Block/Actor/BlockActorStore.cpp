#include "World/Level/Block/Actor/BlockActorStore.h"

#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BeehiveBlockActor.h"
#include "World/Level/Block/Actor/BellBlockActor.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/Actor/FlowerPotBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/CommandBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/CopperGolemStatueBlockActor.h"
#include "World/Level/Block/Actor/DataDrivenBlockActor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/EndGatewayBlockActor.h"
#include "World/Level/Block/Actor/EnderChestBlockActor.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Actor/ItemFrameBlockActor.h"
#include "World/Level/Block/Actor/LodestoneBlockActor.h"
#include "World/Level/Block/Actor/PistonArmBlockActor.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
#include "World/Level/Level.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"

#include <utility>

void BlockActorStore::moveStateFrom(BlockActorStore &&other) {
    mBlockActors = std::move(other.mBlockActors);
    mUnknownBlockActors = std::move(other.mUnknownBlockActors);
    mChunkIndex = std::move(other.mChunkIndex);
    mLoadedChunks = std::move(other.mLoadedChunks);

    for (auto &entry: mBlockActors)
        entry.second->setLevel(mLevel);
}

int64_t BlockActorStore::packPosition(const Vector3i &position) {
    return ((int64_t) position.x & 0x3ffffff) << 38 | ((int64_t) position.y & 0xfff) << 26
           | ((int64_t) position.z & 0x3ffffff);
}

namespace {
    int64_t packChunk(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }

    // packPosition's x and z, sign extended back from their 26 bits
    int64_t chunkOfKey(int64_t key) {
        const int32_t x = (int32_t) (key >> 38);
        const int32_t z = (int32_t) ((uint32_t) (key & 0x3ffffff) << 6) >> 6;
        return packChunk(x >> 4, z >> 4);
    }

    Vector3i positionOf(const Tag &data) {
        return Vector3i(data.getInt("x"), data.getInt("y"), data.getInt("z"));
    }

    void writeSpawnData(BinaryStream &stream, const Tag &blockActor) {
        Tag spawnData = blockActor;
        spawnData.remove("Items");
        spawnData.remove("LootTable");
        spawnData.remove("LootTableSeed");
        NbtIo::writeTag(stream, spawnData, NbtVariant::Network);
    }
}

void BlockActorStore::_index(int64_t key) {
    mChunkIndex[chunkOfKey(key)].insert(key);
}

void BlockActorStore::_unindex(int64_t key) {
    if (mBlockActors.count(key) != 0 || mUnknownBlockActors.count(key) != 0)
        return;

    const auto found = mChunkIndex.find(chunkOfKey(key));
    if (found == mChunkIndex.end())
        return;

    found->second.erase(key);
    if (found->second.empty())
        mChunkIndex.erase(found);
}

const std::unordered_set<int64_t> *BlockActorStore::_positionsIn(int32_t chunkX, int32_t chunkZ) const {
    const auto found = mChunkIndex.find(packChunk(chunkX, chunkZ));
    return found == mChunkIndex.end() ? nullptr : &found->second;
}

BlockActor *BlockActorStore::find(const Vector3i &position) {
    auto it = mBlockActors.find(packPosition(position));
    return it == mBlockActors.end() ? nullptr : it->second.get();
}

void BlockActorStore::insert(std::unique_ptr<BlockActor> blockActor) {
    if (blockActor == nullptr)
        return;

    blockActor->setLevel(mLevel);
    const int64_t key = packPosition(blockActor->getPosition());
    mUnknownBlockActors.erase(key);
    mBlockActors[key] = std::move(blockActor);
    _index(key);
}

void BlockActorStore::remove(const Vector3i &position) {
    const int64_t key = packPosition(position);
    mBlockActors.erase(key);
    mUnknownBlockActors.erase(key);
    _unindex(key);
}

void BlockActorStore::onBlockReplaced(const Vector3i &position) {
    if (mUnknownBlockActors.empty())
        return;

    const int64_t key = packPosition(position);
    if (mUnknownBlockActors.erase(key) != 0)
        _unindex(key);
}

std::unique_ptr<BlockActor> BlockActorStore::take(const Vector3i &position) {
    const auto found = mBlockActors.find(packPosition(position));
    if (found == mBlockActors.end())
        return nullptr;

    const int64_t key = found->first;
    std::unique_ptr<BlockActor> taken = std::move(found->second);
    mBlockActors.erase(found);
    _unindex(key);
    return taken;
}

void BlockActorStore::tick(ServerNetworkHandler &owner) {
    // key snapshot: a ticking actor (hopper, piston) can add or remove others
    std::vector<int64_t> keys;
    keys.reserve(mBlockActors.size());
    for (const auto &entry: mBlockActors)
        keys.push_back(entry.first);

    for (const int64_t key: keys) {
        const auto found = mBlockActors.find(key);
        if (found != mBlockActors.end() && !found->second->tick(owner)) {
            mBlockActors.erase(key);
            _unindex(key);
        }
    }
}

std::vector<Tag> BlockActorStore::saveChunk(int32_t chunkX, int32_t chunkZ) {
    std::vector<Tag> saved;
    const std::unordered_set<int64_t> *positions = _positionsIn(chunkX, chunkZ);
    if (positions == nullptr)
        return saved;

    std::vector<int64_t> dropped;
    for (const int64_t key: *positions) {
        const auto known = mBlockActors.find(key);
        if (known != mBlockActors.end()) {
            saved.push_back(known->second->saveWithPosition());
            continue;
        }

        const auto unknown = mUnknownBlockActors.find(key);
        if (unknown == mUnknownBlockActors.end())
            continue;

        // chunk writes replayed past setBlockState() can replace the block too
        const Vector3i position = positionOf(unknown->second.mData);
        const BlockState *block = mLevel->peekBlockPtr(position.x, position.y, position.z);
        if (block != nullptr && unknown->second.mBlock.empty())
            unknown->second.mBlock = block->mName;
        if (block != nullptr && block->mName != unknown->second.mBlock) {
            dropped.push_back(key);
            continue;
        }

        saved.push_back(unknown->second.mData);
    }

    for (const int64_t key: dropped) {
        mUnknownBlockActors.erase(key);
        _unindex(key);
    }

    return saved;
}

std::string BlockActorStore::encodeChunkNetwork(int32_t chunkX, int32_t chunkZ) const {
    return encodeSubChunkNetwork(chunkX, INT32_MIN, chunkZ);
}

// sectionY INT32_MIN: the whole column
std::string BlockActorStore::encodeSubChunkNetwork(int32_t chunkX, int32_t sectionY, int32_t chunkZ) const {
    BinaryStream stream;
    const std::unordered_set<int64_t> *positions = _positionsIn(chunkX, chunkZ);
    if (positions == nullptr)
        return stream.getBuffer();

    for (const int64_t key: *positions) {
        const auto known = mBlockActors.find(key);
        if (known != mBlockActors.end()) {
            if (sectionY == INT32_MIN || (known->second->getPosition().y >> 4) == sectionY)
                NbtIo::writeTag(stream, known->second->getSpawnCompound(), NbtVariant::Network);
            continue;
        }

        const auto unknown = mUnknownBlockActors.find(key);
        if (unknown != mUnknownBlockActors.end()
            && (sectionY == INT32_MIN || (positionOf(unknown->second.mData).y >> 4) == sectionY))
            writeSpawnData(stream, unknown->second.mData);
    }

    return stream.getBuffer();
}

std::string BlockActorStore::encodeNetwork(const std::vector<Tag> &blockActors) {
    BinaryStream stream;
    for (const Tag &blockActor: blockActors) {
        if (blockActor.isCompound())
            writeSpawnData(stream, blockActor);
    }

    return stream.getBuffer();
}

std::string BlockActorStore::encodeNetworkForSection(const std::vector<Tag> &blockActors, int32_t sectionY) {
    BinaryStream stream;
    for (const Tag &blockActor: blockActors) {
        if (!blockActor.isCompound())
            continue;

        if ((blockActor.getInt("y") >> 4) == sectionY)
            writeSpawnData(stream, blockActor);
    }

    return stream.getBuffer();
}

bool BlockActorStore::isChunkLoaded(int32_t chunkX, int32_t chunkZ) const {
    return mLoadedChunks.count(packChunk(chunkX, chunkZ)) != 0;
}

void BlockActorStore::unloadChunk(int32_t chunkX, int32_t chunkZ) {
    const auto found = mChunkIndex.find(packChunk(chunkX, chunkZ));
    if (found != mChunkIndex.end()) {
        for (const int64_t key: found->second) {
            mBlockActors.erase(key);
            mUnknownBlockActors.erase(key);
        }
        mChunkIndex.erase(found);
    }

    mLoadedChunks.erase(packChunk(chunkX, chunkZ));
}

void BlockActorStore::loadChunk(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockActors,
                                const PacketCodecContext &context) {
    if (!mLoadedChunks.insert(packChunk(chunkX, chunkZ)).second)
        return;

    addLoaded(blockActors, context);
}

void BlockActorStore::addLoaded(const std::vector<Tag> &blockActors, const PacketCodecContext &context) {
    for (const Tag &data: blockActors) {
        if (data.getType() != Tag::Type::Compound)
            continue;

        const Vector3i position = positionOf(data);
        std::unique_ptr<BlockActor> blockActor = create(data.getString("id"));
        if (blockActor == nullptr) {
            // no class here, or its plugin is gone: the game, or that plugin, still owns this data
            const BlockState *block = mLevel->peekBlockPtr(position.x, position.y, position.z);
            const int64_t key = packPosition(position);
            if (mBlockActors.count(key) != 0)
                continue;
            mUnknownBlockActors[key] = UnknownBlockActor{data, block != nullptr ? block->mName : std::string()};
            _index(key);
            continue;
        }

        blockActor->setPosition(position);
        blockActor->loadNbt(data, context);
        blockActor->loadLootTable(data);
        blockActor->loadDynamicProperties(data);
        blockActor->loadEntityStorage(data);
        insert(std::move(blockActor));
    }
}

FALCON_REGISTER_BLOCK_ACTOR(ChestBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(FurnaceBlockActor);

namespace {
    template <FurnaceKind KIND>
    std::unique_ptr<BlockActor> createFurnace() {
        std::unique_ptr<FurnaceBlockActor> furnace = std::make_unique<FurnaceBlockActor>();
        furnace->mKind = KIND;
        return furnace;
    }

    const BlockActorClassRegistry::Registration BLAST_FURNACE_REGISTRATION(FurnaceBlockActor::BLAST_FURNACE_ID,
                                                                           &createFurnace<FurnaceKind::BlastFurnace>);
    const BlockActorClassRegistry::Registration SMOKER_REGISTRATION(FurnaceBlockActor::SMOKER_ID,
                                                                    &createFurnace<FurnaceKind::Smoker>);
}
FALCON_REGISTER_BLOCK_ACTOR(PistonArmBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CommandBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BarrelBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(ShulkerBoxBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(EnderChestBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BedBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CopperGolemStatueBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(HopperBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(DispenserBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(DropperBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BrewingStandBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BeaconBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(EnchantTableBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CrafterBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CampfireBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(LecternBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(ChiseledBookshelfBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(DecoratedPotBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(JukeboxBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(ShelfBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(ItemFrameBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(GlowItemFrameBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BeehiveBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(DataDrivenBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(EndGatewayBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BellBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(StructureBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(LodestoneBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BannerBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(FlowerPotBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(CauldronBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(SignBlockActor);
FALCON_REGISTER_BLOCK_ACTOR(BrushableBlockActor);

namespace {
    std::unique_ptr<BlockActor> createHangingSign() {
        return std::make_unique<SignBlockActor>(true);
    }

    const BlockActorClassRegistry::Registration HANGING_SIGN_REGISTRATION(SignBlockActor::HANGING_ID,
                                                                          &createHangingSign);
}

std::unique_ptr<BlockActor> BlockActorStore::create(const std::string &blockActorId) {
    return BlockActorClassRegistry::create(blockActorId);
}
