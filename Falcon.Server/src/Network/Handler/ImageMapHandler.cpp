#include "Network/Handler/ImageMapHandler.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/MapHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/MapItem.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ItemFrameBlockActor.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/ItemFrameBlock.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

namespace {
    const char *const AIR = "minecraft:air";
    const char *const FRAME = "minecraft:frame";
    const char *const GLOW_FRAME = "minecraft:glow_frame";

    std::string describe(const Vector3i &position) {
        return std::to_string(position.x) + " " + std::to_string(position.y) + " " + std::to_string(position.z);
    }

    int32_t frameFacing(const BlockState &state) {
        return state.mStates.getInt("facing_direction", -1);
    }

    bool isSolid(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid && !ItemFrameBlock::matches(state.mName);
    }

    bool isPicture(Level &mapLevel, const ItemStack &item) {
        if (!MapItem::isFilledMap(item))
            return false;

        const MapItemSavedData *data = mapLevel.getMapData(MapItem::getMapId(item));
        return data != nullptr && data->isImage();
    }

    bool frameState(bool glowing, int32_t facing, BlockState &out) {
        const char *identifier = glowing ? GLOW_FRAME : FRAME;
        BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        registry.initialize();
        const Tag *defaults = registry.getDefaultStates(identifier);
        if (defaults == nullptr)
            return false;

        Tag states = *defaults;
        states.putInt("facing_direction", facing);
        states.putByte("item_frame_map_bit", 1);
        out = BlockState(identifier, states);
        return true;
    }

    MapItemSavedData *findImage(World &world, int64_t mapId) {
        MapItemSavedData *data = world.getOverworld().getMapData(mapId);
        return data != nullptr && data->isImage() ? data : nullptr;
    }
}

std::vector<int64_t> ImageMapHandler::createMaps(ServerNetworkHandler &owner, World &world,
                                                 const std::vector<ImageMapTile> &tiles) {
    MapDataManager &maps = world.getOverworld().getMaps();
    std::vector<int64_t> ids;
    ids.reserve(tiles.size());
    for (const ImageMapTile &tile: tiles) {
        if (tile.size() != (size_t) MapItemSavedData::PIXEL_COUNT)
            continue;

        MapItemSavedData &data = maps.createMap(owner.allocateActorUniqueId());
        data.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        data.setLocked(true);
        data.setImage(true);
        MapPixelArea changed;
        data.writePixels(0, 0, MapItemSavedData::MAP_SIZE, MapItemSavedData::MAP_SIZE, tile.data(), changed);
        data.refreshUnexploredPixels();
        ids.push_back(data.getMapId());
    }
    return ids;
}

bool ImageMapHandler::canPlace(Level &level, const Vector3i &topLeft, int32_t facing, int32_t columns,
                               int32_t rows, std::string &error) {
    std::vector<Vector3i> positions;
    if (!ImageMap::wallPositions(topLeft, facing, columns, rows, positions)) {
        error = "a wall needs a facing from 2 to 5 and at most " + std::to_string(ImageMap::MAX_GRID_SIDE)
                + " maps a side";
        return false;
    }

    Level &mapLevel = level.getOwner() != nullptr ? level.getOwner()->getWorldOf(level).getOverworld() : level;
    for (const Vector3i &position: positions) {
        if (position.y < level.getMinY() || position.y > level.getMaxY()
            || !level.isChunkResident(position.x >> 4, position.z >> 4)) {
            error = "no loaded block at " + describe(position);
            return false;
        }

        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (ItemFrameBlock::matches(state.mName)) {
            const ItemFrameBlockActor *frame = level.getBlockActors().find<ItemFrameBlockActor>(position);
            if (frameFacing(state) != facing
                || (frame != nullptr && !frame->isEmpty() && !isPicture(mapLevel, frame->getItem()))) {
                error = "the frame at " + describe(position) + " faces elsewhere or holds an item";
                return false;
            }
            continue;
        }

        const Vector3i support = ImageMap::supportOf(position, facing);
        if (state.mName != AIR || !isSolid(level.getBlockState(support.x, support.y, support.z))) {
            error = "a frame at " + describe(position) + " needs air and a solid block behind it";
            return false;
        }
    }
    return true;
}

bool ImageMapHandler::place(ServerNetworkHandler &owner, Level &level, const Vector3i &topLeft, int32_t facing,
                            const std::vector<int64_t> &mapIds, int32_t columns, int32_t rows, bool glowing,
                            std::string &error) {
    if (mapIds.size() != (size_t) columns * (size_t) rows || mapIds.empty()) {
        error = "the grid needs one map per frame";
        return false;
    }

    World &world = owner.getWorldOf(level);
    for (const int64_t mapId: mapIds) {
        if (findImage(world, mapId) == nullptr) {
            error = "map " + std::to_string(mapId) + " is not an image map of this world";
            return false;
        }
    }

    BlockState placedState;
    if (!canPlace(level, topLeft, facing, columns, rows, error) || !frameState(glowing, facing, placedState)) {
        if (error.empty())
            error = "item frames are missing from the block palette";
        return false;
    }

    // made before the first frame: a failure here leaves the wall untouched
    const ItemStack blankMap = owner.createItemStack(MapItem::IDENTIFIER, 1);
    if (blankMap.isAir()) {
        error = "filled maps are missing from the item registry";
        return false;
    }

    std::vector<Vector3i> positions;
    ImageMap::wallPositions(topLeft, facing, columns, rows, positions);
    BlockActorStore &blockActors = level.getBlockActors();
    for (size_t index = 0; index < positions.size(); ++index) {
        const Vector3i &position = positions[index];
        BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (!ItemFrameBlock::matches(state.mName)) {
            level.setBlock(position, placedState, false);
            state = placedState;
        } else if (state.mStates.getByte("item_frame_map_bit", 0) == 0) {
            Tag states = state.mStates;
            states.putByte("item_frame_map_bit", 1);
            state = BlockState(state.mName, states);
            level.setBlock(position, state, false);
        }

        ItemFrameBlockActor *frame = blockActors.find<ItemFrameBlockActor>(position);
        if (frame == nullptr) {
            std::unique_ptr<BlockActor> created = ItemFrameBlock::createBlockActor(state.mName);
            created->setPosition(position);
            created->setState(state);
            frame = static_cast<ItemFrameBlockActor *>(created.get());
            blockActors.insert(std::move(created));
        }

        ItemStack map = blankMap;
        MapItem::setMapId(map, *findImage(world, mapIds[index]));

        const ItemStack previous = frame->getItem();
        MapHandler::onFrameItemChanged(owner, level, position, previous, map);
        frame->setItem(std::move(map));
        frame->setRotation(0);
        BlockActionHandler::broadcastBlockActorData(owner, level, *frame);
    }
    return true;
}

bool ImageMapHandler::give(ServerNetworkHandler &owner, ServerPlayer &player, int64_t mapId) {
    MapItemSavedData *data = findImage(owner.getWorldFor(player), mapId);
    if (data == nullptr)
        return false;

    ItemStack map = owner.createItemStack(MapItem::IDENTIFIER, 1);
    if (map.isAir())
        return false;

    map.mTag = Tag::ofCompound();
    MapItem::setMapId(map, *data);

    std::vector<int> touched;
    if (!player.getInventory().addItem(map, touched))
        owner._throwItem(player, map);
    for (const int slot: touched)
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    return true;
}

bool ImageMapHandler::update(ServerNetworkHandler &owner, World &world, int64_t mapId, int32_t x, int32_t y,
                             int32_t width, int32_t height, const uint32_t *pixels, std::string &error) {
    MapItemSavedData *data = findImage(world, mapId);
    if (data == nullptr) {
        error = "map " + std::to_string(mapId) + " is not an image map of this world";
        return false;
    }

    if (pixels == nullptr || width <= 0 || height <= 0 || x < 0 || y < 0 || x > MapItemSavedData::MAP_SIZE - width
        || y > MapItemSavedData::MAP_SIZE - height) {
        error = "the area must lie inside the 128 x 128 map";
        return false;
    }

    MapPixelArea changed;
    if (!data->writePixels(x, y, width, height, pixels, changed))
        return true;

    const ClientboundMapItemDataPacket packet = data->buildAreaPacket(changed);
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent || &owner.getWorldFor(player) != &world)
            continue;

        if (player.getMapRequests().isViewing(mapId))
            owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    }
    return true;
}

bool ImageMapHandler::isImageMap(World &world, int64_t mapId) {
    return findImage(world, mapId) != nullptr;
}
