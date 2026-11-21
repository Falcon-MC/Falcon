#include "Core/NBT/Tag.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Inventory/CartographyRecipes.h"
#include "World/Item/MapItem.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Map/MapColors.h"
#include "World/Level/Map/MapDataManager.h"
#include "World/Level/Map/MapItemSavedData.h"
#include "World/Level/Map/MapRenderer.h"
#include "World/Level/Map/MapRequestQueue.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack stack(const char *identifier, int count = 1, int damage = 0) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = count;
        item.mDamage = damage;
        return item;
    }

    ItemStack filledMap(int64_t mapId, int damage = 0) {
        ItemStack item = stack(MapItem::IDENTIFIER, 1, damage);
        item.mTag = Tag::ofCompound();
        item.mTag.putLong(MapItem::TAG_MAP_ID, mapId);
        return item;
    }

    bool closeTo(uint32_t color, int red, int green, int blue) {
        return std::abs((int) ((color >> 16) & 0xff) - red) <= 1 && std::abs((int) ((color >> 8) & 0xff) - green) <= 1
               && std::abs((int) (color & 0xff) - blue) <= 1;
    }

    class FlatSource final : public MapRenderSource {
    public:
        FlatSource(const char *surface, int32_t surfaceY) : mSurface(surface), mSurfaceY(surfaceY) {}

        bool hasChunk(int32_t chunkX, int32_t chunkZ) override {
            (void) chunkZ;
            return chunkX < mMissingFromChunkX;
        }

        int32_t getSurfaceY(int32_t x, int32_t z) override {
            (void) z;
            return x >= mStepX ? mSurfaceY + 2 : mSurfaceY + 1;
        }

        const BlockState &getBlock(int32_t x, int32_t y, int32_t z) override {
            (void) z;
            const int32_t top = x >= mStepX ? mSurfaceY + 1 : mSurfaceY;
            if (y > top)
                return mAir;
            if (y > top - mWaterDepth)
                return mWater;
            return mSurface;
        }

        MapBiomeColors getBiome(int32_t x, int32_t y, int32_t z) override {
            (void) x;
            (void) y;
            (void) z;
            MapBiomeColors plains;
            plains.mTemperature = 0.8f;
            plains.mDownfall = 0.4f;
            return plains;
        }

        int32_t getMinY() const override {
            return -64;
        }

        int32_t getMaxY() const override {
            return 320;
        }

        bool hasCeiling() const override {
            return mCeiling;
        }

        BlockState mSurface;
        BlockState mWater = BlockState("minecraft:water");
        BlockState mAir;
        int32_t mSurfaceY;
        int32_t mWaterDepth = 0;
        int32_t mStepX = 1 << 30;
        int32_t mMissingFromChunkX = 1 << 26;
        bool mCeiling = false;
    };

    void savedDataFormat() {
        MapItemSavedData map(-12884901887LL);
        map.setScale(2);
        map.setParentMapId(-12884901886LL);
        map.setOrigin(Vector3f(100.0f, 70.0f, -300.0f), 2, 0);
        map.setPixel(3, 2, 0xff336699u);
        map.setLocked(true);
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        map.addDecoration(MapDecorationType::MarkerWhite, MapItemTrackedActorKey::entity(7),
                          Vector3f(100.0f, 0.0f, -300.0f), "", white, 0);
        map.addDecoration(MapDecorationType::MarkerGreen, MapItemTrackedActorKey::block(Vector3i(110, 64, -290)),
                          Vector3f(110.0f, 0.0f, -290.0f), "", white, 0);

        Tag tag = Tag::ofCompound();
        map.save(tag);
        require(tag.contains("mapId", Tag::Type::Long) && tag.getLong("mapId") == -12884901887LL, "mapId is a long");
        require(tag.getLong("parentMapId") == -12884901886LL, "parentMapId is a long");
        require(tag.contains("dimension", Tag::Type::Byte) && tag.contains("scale", Tag::Type::Byte),
                "dimension and scale are bytes");
        require(tag.contains("xCenter", Tag::Type::Int) && tag.contains("zCenter", Tag::Type::Int), "centres are ints");
        require(tag.getShort("width") == 128 && tag.getShort("height") == 128, "width and height are shorts of 128");
        require(tag.get("colors")->asByteArray().size() == 65536, "colours keep four bytes per pixel");
        require(tag.getByte("mapLocked") == 1 && tag.contains("unlimitedTracking", Tag::Type::Byte)
                        && tag.contains("fullyExplored", Tag::Type::Byte),
                "flags are bytes");
        require(!tag.contains("previewIncomplete"), "a complete preview writes no flag");
        const std::vector<Tag> &decorations = tag.get("decorations")->getList();
        require(decorations.size() == 1, "player markers are not saved");
        require(decorations[0].get("key")->getInt("type") == 1 && decorations[0].get("key")->getInt("blockX") == 110,
                "a frame marker saves its block");
        require(decorations[0].get("data")->getInt("type") == 1, "the decoration keeps its type");

        MapItemSavedData loaded(-12884901887LL);
        loaded.load(tag);
        require(loaded.getScale() == 2 && loaded.getParentMapId() == -12884901886LL && loaded.isLocked(),
                "load restores the fields");
        require(loaded.getOrigin().x == map.getOrigin().x && loaded.getOrigin().z == map.getOrigin().z,
                "load restores the centre");
        require(loaded.getPixel(3, 2) == 0xff336699u, "load restores the pixels");
        require(loaded.getDecorations().size() == 1, "load restores saved decorations");

        Tag widest = Tag::ofCompound();
        widest.putByte("scale", 9);
        widest.putLong("parentMapId", 5);
        MapItemSavedData clamped(1);
        clamped.load(widest);
        require(clamped.getScale() == 4 && clamped.getParentMapId() == MapItemSavedData::INVALID_ID,
                "a scale past 4 clamps and loses its parent");
    }

    void centreGrid() {
        MapItemSavedData map(1);
        map.setOrigin(Vector3f(10.0f, 0.0f, 10.0f), 0, 0);
        require(map.getOrigin().x == 0 && map.getOrigin().z == 0, "scale 0 maps start at 0");
        map.setOrigin(Vector3f(70.0f, 0.0f, -70.0f), 0, 0);
        require(map.getOrigin().x == 128 && map.getOrigin().z == -128, "the grid steps by 128 around -64");
        map.setOrigin(Vector3f(10.0f, 0.0f, 300.0f), 1, 0);
        require(map.getOrigin().x == 64 && map.getOrigin().z == 320, "scale 1 steps by 256");
        map.setOrigin(Vector3f(-1000.0f, 0.0f, 1000.0f), 4, 0);
        require(map.getOrigin().x == -1088 && map.getOrigin().z == 960, "scale 4 steps by 2048");
    }

    void scaleChain() {
        MapDataManager maps;
        int64_t next = 100;
        MapItemSavedData &map = maps.createMapSavedData([&next]() { return next++; }, Vector3f(10.0f, 64.0f, 10.0f), 0,
                                                        0);
        require(map.getMapId() == 100 && map.getScale() == 0, "the requested scale is returned");

        MapItemSavedData *parent = maps.findLoaded(map.getParentMapId());
        require(map.getParentMapId() == 101 && parent != nullptr && parent->getScale() == 1, "each scale has a parent");
        require(maps.findLoaded(104)->getParentMapId() == MapItemSavedData::INVALID_ID, "scale 4 has no parent");
        require(maps.getMapChain(nullptr, map).size() == 5, "the chain lists every scale");

        map.setOrigin(Vector3f(300.0f, 0.0f, 10.0f), 0, 0);
        require(maps.expandMap(nullptr, 100, false) == 101, "zooming out moves to the parent");
        require(parent->getOrigin().x == 320, "the parent recentres on the child");

        map.setPixel(5, 5, 0xff112233u);
        MapItemSavedData *copy = maps.copyAndLockMap(nullptr, 100, 777);
        require(copy != nullptr && copy->isLocked() && copy->getMapId() == 777, "the copy is locked under its id");
        require(copy->getPixel(5, 5) == 0xff112233u && copy->getScale() == 0, "the copy keeps the pixels");
        require(!map.isLocked(), "the original stays unlocked");
    }

    void colours() {
        require(closeTo(MapColors::grassColor(0.8f, 0.4f), 145, 189, 89), "plains grass");
        require(closeTo(MapColors::foliageColor(0.8f, 0.4f), 119, 171, 47), "plains foliage");
        require(MapColors::find("minecraft:stone") != nullptr && MapColors::find("minecraft:stone")->mColor[0] == 112,
                "stone is grey");
        require(MapColors::find("minecraft:glass")->mColor[3] == 0, "glass has no colour");

        const float stone[4] = {112.0f / 255.0f, 112.0f / 255.0f, 112.0f / 255.0f, 1.0f};
        require(MapRenderer::shade(stone, 0) == 0xff4f4f4fu, "low shade");
        require(MapRenderer::shade(stone, 1) == 0xff606060u, "normal shade");
        require(MapRenderer::shade(stone, 2) == 0xff707070u, "high shade");

        BlockState log("minecraft:oak_log");
        log.mStates.putString("pillar_axis", "x");
        float color[4];
        MapColors::getColor(log, MapBiomeColors(), color);
        require((int) (color[0] * 255.0f + 0.5f) == 129, "a fallen log shows its bark");
    }

    void rendering() {
        FlatSource stone("minecraft:stone", 64);
        MapItemSavedData map(1);
        map.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        MapRenderer::update(map, stone, Vector3i(0, 64, 0));
        require(map.getPixel(64, 64) == 0xff606060u, "flat stone renders at normal shade");
        require(map.getPixel(64, 0) == 0xff606060u && map.getPixel(1, 64) == 0xff606060u, "the whole radius renders");

        FlatSource stepped("minecraft:stone", 64);
        stepped.mStepX = 10;
        MapItemSavedData steps(2);
        steps.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        MapRenderer::update(steps, stepped, Vector3i(0, 64, 0));
        require(steps.getPixel(74, 64) == 0xff606060u, "the shade compares against the pixel north");

        FlatSource sea("minecraft:sand", 60);
        sea.mWaterDepth = 3;
        MapItemSavedData ocean(3);
        ocean.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        MapRenderer::update(ocean, sea, Vector3i(0, 64, 0));
        const uint32_t shallow = ocean.getPixel(64, 64);
        require(((shallow >> 16) & 0xff) > (shallow & 0xff), "water is blue");

        FlatSource missing("minecraft:stone", 64);
        missing.mMissingFromChunkX = 0;
        MapItemSavedData unloaded(4);
        unloaded.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        MapRenderer::update(unloaded, missing, Vector3i(0, 64, 0));
        require(unloaded.getPixel(64, 64) == 0, "a column without its chunk stays blank");

        FlatSource nether("minecraft:netherrack", 64);
        nether.mCeiling = true;
        MapItemSavedData ceiling(5);
        ceiling.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 1);
        MapRenderer::update(ceiling, nether, Vector3i(0, 64, 0));
        require(ceiling.getPixel(64, 64) != 0 && ceiling.getPixel(0, 0) == 0, "the Nether draws half the radius");
    }

    void decorations() {
        MapItemSavedData map(1);
        map.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const MapItemTrackedActorKey player = MapItemTrackedActorKey::entity(9);

        map.addDecoration(MapDecorationType::MarkerWhite, player, Vector3f(10.0f, 90.0f, -20.0f), "", white, 0);
        require(map.getDecorations().size() == 1, "a player inside the map shows");
        const MapDecorationEntry &inside = map.getDecorations()[0];
        require(inside.mX == 20 && inside.mY == -40 && inside.mRotation == 4, "markers use half block steps");

        map.addDecoration(MapDecorationType::MarkerWhite, player, Vector3f(200.0f, 0.0f, 0.0f), "", white, 0);
        require(map.getDecorations()[0].mType == MapDecorationType::SquareWhite && map.getDecorations()[0].mX == 127,
                "a player past the edge turns into a square at the edge");

        map.addDecoration(MapDecorationType::MarkerWhite, player, Vector3f(400.0f, 0.0f, 0.0f), "", white, 0);
        require(map.getDecorations().empty(), "a far player disappears");

        map.setUnlimitedTracking(true);
        map.addDecoration(MapDecorationType::MarkerWhite, player, Vector3f(400.0f, 0.0f, 0.0f), "", white, 0);
        require(map.getDecorations()[0].mType == MapDecorationType::SmallSquareWhite,
                "unlimited tracking keeps far players");

        map.addDecoration(MapDecorationType::Monument, MapItemTrackedActorKey::block(Vector3i(8, 0, 8)),
                          Vector3f(8.0f, 123.0f, 8.0f), "", white, 0);
        require(map.getDecorations()[1].mRotation == 8, "structure markers never turn");

        MapItemSavedData frames(2);
        frames.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        frames.addDecoration(MapDecorationType::MarkerGreen, MapItemTrackedActorKey::block(Vector3i(500, 0, 0)),
                             Vector3f(500.0f, 0.0f, 0.0f), "", white, 0);
        require(frames.getDecorations().empty(), "a frame outside the map is not drawn");
    }

    void updatePackets() {
        MapItemSavedData map(1);
        MapItemTrackingInfo &tracking = map.addTracked(MapItemTrackedActorKey::entity(3), 0,
                                                       MapDecorationType::MarkerWhite);
        ClientboundMapItemDataPacket packet;
        require(map.buildUpdatePacket(tracking, packet) && packet.mHasColors && packet.mWidth == 128,
                "a new viewer gets the whole map");

        require(map.buildUpdatePacket(tracking, packet) && !packet.mHasColors && packet.mHasDecorations,
                "then decorations alone");
        for (int call = 0; call < 4; ++call)
            require(!map.buildUpdatePacket(tracking, packet), "decorations wait for the fifth call");

        map.setPixel(10, 20, 0xff000001u);
        map.setPixel(12, 21, 0xff000002u);
        require(map.buildUpdatePacket(tracking, packet) && packet.mXOffset == 10 && packet.mYOffset == 20
                        && packet.mWidth == 3 && packet.mHeight == 2 && packet.mColors.size() == 6,
                "a change sends the area it covers");
        require(packet.mColors[0] == (int32_t) 0xff000001u && packet.mColors[5] == (int32_t) 0xff000002u,
                "the area is sent row by row");

        const ClientboundMapItemDataPacket creation = map.buildCreationPacket({1, 2});
        require(creation.mHasTrackedActorIds && creation.mTrackedActorIds.size() == 2 && !creation.mHasColors,
                "the creation packet links the scales");
    }

    void cartography() {
        MapDataManager maps;
        int64_t next = 50;
        maps.createMapSavedData([&next]() { return next++; }, Vector3f(0.0f, 0.0f, 0.0f), 0, 0);

        CartographyRecipes::Maps access;
        access.mGetMap = [&maps](int64_t mapId) { return maps.findLoaded(mapId); };
        access.mExpand = [&maps](int64_t mapId, bool scaled) { return maps.expandMap(nullptr, mapId, scaled); };
        access.mAllocateLockId = [](int64_t mapId) {
            (void) mapId;
            return (int64_t) 900;
        };

        const ItemStack map = filledMap(50);
        CartographyResult clone = CartographyRecipes::createResult(map, stack("minecraft:empty_map", 5), nullptr,
                                                                   access);
        require(clone.mResult.mCount == 2 && MapItem::getMapId(clone.mResult) == 50, "an empty map clones the map");
        require(clone.mInputCost == 1 && clone.mAdditionalCost == 1, "cloning takes one of each");

        CartographyResult zoom = CartographyRecipes::createResult(map, stack("minecraft:paper", 3), nullptr, access);
        require(MapItem::getMapId(zoom.mResult) == 51 && zoom.mResult.mTag.getByte(MapItem::TAG_IS_INIT) == 1,
                "paper zooms out to the parent");
        require(zoom.mAdditionalCost == 1, "zooming takes one paper");
        require(CartographyRecipes::createResult(filledMap(54), stack("minecraft:paper"), nullptr, access)
                        .mResult.isAir(),
                "the widest scale cannot zoom");

        CartographyResult locked = CartographyRecipes::createResult(map, stack("minecraft:glass_pane"), nullptr,
                                                                    access);
        require(MapItem::getMapId(locked.mResult) == 900 && locked.mResult.mDamage == (int) MapItemType::Locked,
                "a glass pane locks a copy");
        require(locked.mLock.mOriginalId == 50 && locked.mLock.mCopyId == 900, "the result names the copy to make");
        require(maps.findLoaded(900) == nullptr, "a result alone makes no map: the request may still fail");
        maps.copyAndLockMap(nullptr, locked.mLock.mOriginalId, locked.mLock.mCopyId);
        require(clone.mLock.mCopyId == MapItemSavedData::INVALID_ID, "cloning makes no copy");
        require(CartographyRecipes::isLocking(map, stack("minecraft:glass_pane"), 50),
                "the client lock packet matches a map and a glass pane");
        require(!CartographyRecipes::isLocking(map, stack("minecraft:paper"), 50)
                        && !CartographyRecipes::isLocking(map, stack("minecraft:glass_pane"), 51)
                        && !CartographyRecipes::isLocking(filledMap(900, 6), stack("minecraft:glass_pane"), 900),
                "the client lock packet needs the table about to lock that map");
        require(CartographyRecipes::createResult(filledMap(900, 6), stack("minecraft:glass_pane"), nullptr, access)
                        .mResult.isAir(),
                "a locked map cannot lock again");
        require(CartographyRecipes::createResult(filledMap(900, 6), stack("minecraft:paper"), nullptr, access)
                        .mResult.isAir(),
                "a locked map cannot zoom");

        CartographyResult locator = CartographyRecipes::createResult(map, stack("minecraft:compass"), nullptr, access);
        require(MapItem::displaysPlayers(locator.mResult), "a compass turns a map into a locator map");
        CartographyResult emptyLocator = CartographyRecipes::createResult(stack("minecraft:empty_map", 4),
                                                                          stack("minecraft:compass"), nullptr, access);
        require(emptyLocator.mResult.mDamage == 2 && emptyLocator.mResult.mCount == 1,
                "a compass turns an empty map into an empty locator map");

        const std::string name = "Base";
        CartographyResult renamed = CartographyRecipes::createResult(map, ItemStack::air(), &name, access);
        require(renamed.mResult.mTag.get("display")->getString("Name") == "Base" && renamed.mAdditionalCost == 0,
                "the table renames a map for free");
        require(CartographyRecipes::createResult(map, ItemStack::air(), nullptr, access).mResult.isAir(),
                "a lone map makes nothing");
        require(CartographyRecipes::createResult(stack("minecraft:stone"), stack("minecraft:paper"), nullptr, access)
                        .mResult.isAir(),
                "only maps go in the table");
    }

    void requestQueue() {
        MapRequestQueue queue;
        require(queue.push(1) && !queue.push(1), "a waiting id is not queued twice");
        for (int64_t id = 2; queue.push(id); ++id) {
        }
        require(queue.size() == MapRequestQueue::CAPACITY, "the queue stops at its capacity");

        const std::vector<int64_t> due = queue.takeDue();
        require(due.size() == MapRequestQueue::PER_TICK && due[0] == 1 && due[1] == 2, "a tick answers the oldest few");
        require(queue.size() == MapRequestQueue::CAPACITY - MapRequestQueue::PER_TICK, "answered ids leave the queue");
        require(queue.push(1), "an answered id can be asked again");
    }

    void unloading() {
        MapDataManager maps;
        maps.createMap(1).clearDirtyForSave();
        maps.createMap(2).clearDirtyForSave();
        maps.createMap(3);
        maps.createMap(4).clearDirtyForSave();
        maps.findLoaded(4)->addTracked(MapItemTrackedActorKey::block(Vector3i(1, 2, 3)), 0,
                                       MapDecorationType::MarkerGreen);

        maps.unloadUnused();
        require(maps.findLoaded(1) != nullptr, "a map used since the last save stays");
        maps.unloadUnused();
        require(maps.findLoaded(1) != nullptr, "a lookup counts as a use");
        maps.unloadUnused();
        maps.unloadUnused();
        require(maps.findLoaded(2) == nullptr, "a saved map left alone unloads");
        require(maps.findLoaded(3) != nullptr, "an unsaved map stays");
        require(maps.findLoaded(4) != nullptr, "a framed map stays");
    }

    void staleTracking() {
        MapItemSavedData map(1);
        map.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const MapItemTrackedActorKey gone = MapItemTrackedActorKey::entity(5);
        const MapItemTrackedActorKey frame = MapItemTrackedActorKey::block(Vector3i(2, 0, 2));
        map.addTracked(gone, 0, MapDecorationType::MarkerWhite);
        map.addTracked(MapItemTrackedActorKey::entity(6), 0, MapDecorationType::MarkerWhite);
        map.addTracked(frame, 0, MapDecorationType::MarkerGreen);
        map.addDecoration(MapDecorationType::MarkerWhite, gone, Vector3f(1.0f, 0.0f, 1.0f), "", white, 0);
        map.addDecoration(MapDecorationType::MarkerGreen, frame, Vector3f(2.0f, 0.0f, 2.0f), "", white, 0);

        map.refreshTracked(
                [](MapItemTrackingInfo &tracking) { return tracking.mKey == MapItemTrackedActorKey::entity(6); });
        require(map.findTracked(gone) == nullptr && map.findTracked(MapItemTrackedActorKey::entity(6)) != nullptr,
                "an actor gone from the world stops being tracked");
        require(map.findTracked(frame) != nullptr, "a frame stays tracked");
        require(map.getDecorations().empty(), "both lose their marker");
    }
}

int main() {
    try {
        savedDataFormat();
        centreGrid();
        scaleChain();
        colours();
        rendering();
        decorations();
        updatePackets();
        cartography();
        requestQueue();
        unloading();
        staleTracking();
        std::cout << "map data, rendering, decorations and cartography checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
