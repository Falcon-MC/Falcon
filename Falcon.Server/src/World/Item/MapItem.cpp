#include "World/Item/MapItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(MapItem, 100);

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ExplorationMaps.h"
#include "World/Level/Block/Actor/ItemFrameBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/Map/MapRenderer.h"
#include "World/Level/World.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Level/Block/BlockNames.h"

#include <cmath>
#include <unordered_map>

namespace {
    const int64_t RENDER_INTERVAL = 20;
    const int32_t RENDER_INTERVAL_JITTER = 6;
    const float WHITE[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float NETHER_RED[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    const float END_MAGENTA[4] = {1.0f, 0.0f, 1.0f, 1.0f};
    // frame facings 2 to 5 turn the marker by these degrees
    const float FRAME_ROTATIONS[4] = {180.0f, 0.0f, 90.0f, 270.0f};

    class LevelMapSource final : public MapRenderSource {
    public:
        LevelMapSource(Level &level, BiomeRegistry &biomes) : mLevel(level), mBiomes(biomes) {}

        bool hasChunk(int32_t chunkX, int32_t chunkZ) override {
            LevelChunk *chunk = mLevel.peekChunkPtr(chunkX, chunkZ);
            return chunk != nullptr && chunk->isPopulated();
        }

        int32_t getSurfaceY(int32_t x, int32_t z) override {
            LevelChunk *chunk = mLevel.peekChunkPtr(x >> 4, z >> 4);
            if (chunk == nullptr)
                return mLevel.getMinY();

            for (int index = LevelChunk::SUB_CHUNK_COUNT - 1; index >= 0; --index) {
                if (chunk->getSubChunk(index).isEmpty())
                    continue;

                const int32_t baseY = LevelChunk::MIN_Y + index * 16;
                for (int32_t y = baseY + 15; y >= baseY; --y) {
                    if (chunk->getBlock(x & 15, y, z & 15).mName != AIR)
                        return y + 1;
                }
            }
            return mLevel.getMinY();
        }

        const BlockState &getBlock(int32_t x, int32_t y, int32_t z) override {
            LevelChunk *chunk = mLevel.peekChunkPtr(x >> 4, z >> 4);
            if (chunk == nullptr || y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                return mAir;

            return chunk->getBlock(x & 15, y, z & 15);
        }

        MapBiomeColors getBiome(int32_t x, int32_t y, int32_t z) override {
            LevelChunk *chunk = mLevel.peekChunkPtr(x >> 4, z >> 4);
            if (chunk == nullptr)
                return MapBiomeColors();

            return _colorsOf(
                    chunk->getBiomeAt(x & 15, std::max(LevelChunk::MIN_Y, std::min(y, LevelChunk::MAX_Y)), z & 15));
        }

        int32_t getMinY() const override {
            return mLevel.getMinY();
        }

        int32_t getMaxY() const override {
            return mLevel.getMaxY() + 1;
        }

        bool hasCeiling() const override {
            return mLevel.getDimensionType() == DimensionType::Nether;
        }

    private:
        static constexpr const char *AIR = BlockNames::AIR;

        MapBiomeColors _colorsOf(uint32_t biomeId) {
            const auto cached = mCache.find(biomeId);
            if (cached != mCache.end())
                return cached->second;

            const MapBiomeColors colors = MapColors::biomeColors(mBiomes, biomeId);
            mCache.emplace(biomeId, colors);
            return colors;
        }

        Level &mLevel;
        BiomeRegistry &mBiomes;
        const BlockState mAir;
        std::unordered_map<uint32_t, MapBiomeColors> mCache;
    };

    ServerPlayer *findPlayer(ServerNetworkHandler &owner, int64_t uniqueId) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getUniqueId() == uniqueId)
                return &entry.second;
        }
        return nullptr;
    }

    Vector3i floorPosition(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    float frameRotation(Level &level, const Vector3i &position, float previous) {
        if (level.peekChunkPtr(position.x >> 4, position.z >> 4) == nullptr)
            return previous;

        if (level.getBlockActors().find<ItemFrameBlockActor>(position) == nullptr)
            return previous;

        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        const int32_t facing = state.mStates.getInt("facing_direction", -1);
        if (facing < 2 || facing > 5)
            return 0.0f;

        return FRAME_ROTATIONS[facing - 2];
    }
}

MapItem::MapItem(const Item &base) : Item(base) {}

bool MapItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool MapItem::isFilledMap(const ItemStack &item) {
    return !item.isAir() && item.mDefinition->getIdentifier() == IDENTIFIER;
}

int64_t MapItem::getMapId(const ItemStack &item) {
    if (!item.mTag.isCompound() || !item.mTag.contains(TAG_MAP_ID, Tag::Type::Long))
        return MapItemSavedData::INVALID_ID;

    return item.mTag.getLong(TAG_MAP_ID);
}

void MapItem::setMapId(ItemStack &item, const MapItemSavedData &data) {
    if (!item.mTag.isCompound())
        item.mTag = Tag::ofCompound();
    if (data.isLocked())
        item.mDamage = (int32_t) MapItemType::Locked;
    item.mTag.putLong(TAG_MAP_ID, data.getMapId());
}

bool MapItem::displaysPlayers(const ItemStack &item) {
    return item.mTag.isCompound() && item.mTag.getByte(TAG_DISPLAY_PLAYERS, 0) != 0;
}

std::mt19937 &MapItem::random() {
    static std::mt19937 generator(std::random_device{}());
    return generator;
}

Level &MapItem::getMapLevel(ServerNetworkHandler &owner, const Actor &actor) {
    return owner.getWorldFor(actor).getOverworld();
}

bool MapItem::isHoldingMap(const ServerPlayer &player, int64_t mapId) {
    const PlayerInventory &inventory = player.getInventory();
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
        if (isFilledMap(inventory.getItem(slot)) && getMapId(inventory.getItem(slot)) == mapId)
            return true;
    }
    return isFilledMap(inventory.getOffhand()) && getMapId(inventory.getOffhand()) == mapId;
}

bool MapItem::convertPosition(float &x, float &z, int32_t from, int32_t to) {
    if (from == to)
        return true;

    if (from == 2 || to == 2)
        return false;

    const float factor = from == 0 ? 0.125f : 8.0f;
    x *= factor;
    z *= factor;
    return true;
}

bool MapItem::updateTrackedDecoration(ServerNetworkHandler &owner, World &world, MapItemSavedData &data,
                                      MapItemTrackingInfo &tracking) {
    const int32_t mapDimension = data.getDimension();
    float x = 0.0f;
    float z = 0.0f;
    const float *color = WHITE;
    std::string label;

    if (tracking.mKey.mType == MapItemTrackedActorKey::Type::Block) {
        if (tracking.mDimension != mapDimension)
            return false;

        x = (float) tracking.mKey.mPosition.x;
        z = (float) tracking.mKey.mPosition.z;
        Level &level = world.getLevel(Dimension::fromId(mapDimension));
        tracking.mRotation = frameRotation(level, tracking.mKey.mPosition, tracking.mRotation);
    } else {
        ServerPlayer *player = findPlayer(owner, tracking.mKey.mActorId);
        const Actor *actor = player;
        if (actor == nullptr)
            actor = owner.getActor(tracking.mKey.mActorId);
        if (actor == nullptr)
            return false;

        x = actor->getPosition().x;
        z = actor->getPosition().z;
        tracking.mRotation = actor->getRotation().y;

        if (player != nullptr) {
            const int32_t dimension = Dimension::toId(player->getDimension());
            const bool holding = isHoldingMap(*player, data.getMapId());
            if (dimension == 2)
                color = END_MAGENTA;
            else if (dimension == 1)
                color = NETHER_RED;

            // a player who does not carry the map hides from it while invisible or spectating
            MapDecorationType type = MapDecorationType::MarkerWhite;
            if (!holding
                && (player->getFlags().get(ActorFlag::Invisible)
                    || player->isSpectator()))
                type = MapDecorationType::NoDraw;

            if (holding || dimension != mapDimension) {
                if (!convertPosition(x, z, dimension, mapDimension))
                    type = MapDecorationType::NoDraw;
                if (mapDimension == 1 && dimension == 2)
                    type = MapDecorationType::NoDraw;
                if (mapDimension == 2 && dimension != 2)
                    type = MapDecorationType::NoDraw;
            }
            tracking.mDecorationType = type;
        }
    }

    const int32_t noise = (int32_t) (random()() & 0xff);
    data.addDecoration(tracking.mDecorationType, tracking.mKey, Vector3f(x, tracking.mRotation, z), label, color,
                       noise);
    return true;
}

void MapItem::updateTrackedDecorations(ServerNetworkHandler &owner, World &world, MapItemSavedData &data) {
    data.refreshTracked([&owner, &world, &data](MapItemTrackingInfo &tracking) {
        return updateTrackedDecoration(owner, world, data, tracking);
    });
}

void MapItem::tickCarriedBy(ServerNetworkHandler &owner, World &world, MapItemSavedData &data, const ItemStack *item) {
    // every player of the world follows the map
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent || &owner.getWorldFor(player) != &world)
            continue;

        const MapItemTrackedActorKey key = MapItemTrackedActorKey::entity(player.getUniqueId());
        if (data.findTracked(key) == nullptr)
            data.addTracked(key, Dimension::toId(player.getDimension()), MapDecorationType::MarkerWhite);
    }

    updateTrackedDecorations(owner, world, data);

    if (item == nullptr || !item->mTag.isCompound())
        return;

    const Tag *decorations = item->mTag.get("Decorations");
    if (decorations == nullptr || !decorations->isList())
        return;

    for (const Tag &decoration: decorations->getList()) {
        if (!decoration.isCompound())
            break;

        const MapItemTrackedActorKey key = MapItemTrackedActorKey::entity(decoration.getLong("id", 0));
        bool known = false;
        for (const MapDecorationEntry &existing: data.getDecorations())
            known |= existing.mKey == key;
        if (known)
            continue;

        const uint32_t packed = (uint32_t) decoration.getInt("color", 0);
        const float color[4] = {(float) ((packed >> 16) & 0xff) / 255.0f, (float) ((packed >> 8) & 0xff) / 255.0f,
                                (float) (packed & 0xff) / 255.0f, (float) ((packed >> 24) & 0xff) / 255.0f};
        const Vector3f position((float) decoration.getDouble("x", 0.0), (float) decoration.getDouble("rot", 0.0),
                                (float) decoration.getDouble("z", 0.0));
        data.addDecoration((MapDecorationType) (uint8_t) decoration.getByte("type", 0), key, position,
                           decoration.getString("label", std::string()), color, 0);
    }
}

bool MapItem::inventoryTick(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack &item, bool selected) {
    Level &mapLevel = getMapLevel(owner, player);
    MapDataManager &maps = mapLevel.getMaps();
    const int64_t mapId = getMapId(item);

    if (item.mTag.isCompound() && item.mTag.contains(TAG_REGENERATE)) {
        if (const ExplorationMaps::Entry *entry = ExplorationMaps::findByAux(item.mDamage))
            ExplorationMaps::create(owner, item, &player, *entry);
        item.mTag.remove(TAG_REGENERATE);
        return true;
    }

    bool changed = false;
    if (mapId == MapItemSavedData::INVALID_ID) {
        const Vector3i position = floorPosition(player.getPosition());
        MapItemSavedData &created = maps.createMapSavedData(
                [&owner]() { return owner.allocateActorUniqueId(); },
                Vector3f((float) position.x, (float) position.y, (float) position.z),
                Dimension::toId(player.getDimension()), 0);
        setMapId(item, created);
        changed = true;
    }

    // the game looks the map up with the id read before creating one, so a new map starts next tick
    MapItemSavedData *data = maps.getMap(mapLevel.getMapStorage(), mapId);
    if (data == nullptr)
        return changed;

    if (data->isLocked() && item.mDamage != (int32_t) MapItemType::Locked) {
        item.mDamage = (int32_t) MapItemType::Locked;
        changed = true;
    }

    // a picture has nothing to track or draw: the holder only asks for its pixels
    if (data->isImage())
        return changed;

    tickCarriedBy(owner, owner.getWorldFor(player), *data, &item);
    if (!selected || Dimension::toId(player.getDimension()) != data->getDimension())
        return changed;

    // an unexplored locked map keeps rendering until its holes are filled
    const bool settled = !data->hasUnexploredPixels();
    if (settled && data->isLocked())
        return changed;

    int64_t &counter = data->getUpdateCounter();
    if (counter++ != RENDER_INTERVAL)
        return changed;

    counter = (int64_t) (random()() % (uint32_t) RENDER_INTERVAL_JITTER);
    LevelMapSource source(owner.getLevelFor(player), owner.getBiomes());
    MapRenderer::update(*data, source, floorPosition(player.getPosition()));

    if (!settled) {
        MapItemTrackedActorKey key = MapItemTrackedActorKey::entity(player.getUniqueId());
        MapItemTrackingInfo *tracking = data->findTracked(key);
        if (tracking == nullptr)
            tracking = &data->addTracked(key, Dimension::toId(player.getDimension()), MapDecorationType::MarkerWhite);
        tracking->mDirty = true;
        data->refreshUnexploredPixels();
    }
    return changed;
}
