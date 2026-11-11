#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/Handler/ImageMapHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    thread_local std::string gLastError;

    bool fail(const std::string &error) {
        gLastError = error;
        return false;
    }

    World *worldOf(FalconLevel *handle) {
        if (handle == nullptr) {
            fail("no level");
            return nullptr;
        }
        return &owner().getWorldOf(*level(handle));
    }

    bool toFit(FalconImageFit value, ImageMapFit &out) {
        if (value > FALCON_IMAGE_FIT_COVER)
            return fail("fit goes from 0 to 2");

        out = (ImageMapFit) value;
        return true;
    }

    uint32_t createGrid(FalconLevel *handle, const ImageMapImage &image, uint32_t columns, uint32_t rows,
                        FalconImageFit fit, int64_t *mapIds, uint32_t capacity, uint32_t *outColumns,
                        uint32_t *outRows) {
        World *world = worldOf(handle);
        ImageMapFit mode = ImageMapFit::Contain;
        if (world == nullptr || !toFit(fit, mode))
            return 0;
        if (columns > (uint32_t) ImageMap::MAX_GRID_SIDE || rows > (uint32_t) ImageMap::MAX_GRID_SIDE) {
            fail("image maps go up to 32 x 32 maps");
            return 0;
        }

        int32_t gridColumns = (int32_t) columns;
        int32_t gridRows = (int32_t) rows;
        std::string error;
        if (!ImageMap::gridSize(image.mWidth, image.mHeight, gridColumns, gridRows, error)) {
            fail(error);
            return 0;
        }

        const uint32_t count = (uint32_t) (gridColumns * gridRows);
        if (outColumns != nullptr)
            *outColumns = (uint32_t) gridColumns;
        if (outRows != nullptr)
            *outRows = (uint32_t) gridRows;
        // checked before any map exists: a short buffer leaves no orphan maps behind
        if (mapIds == nullptr || capacity < count) {
            fail("the id buffer needs room for " + std::to_string(count) + " maps");
            return 0;
        }

        std::vector<ImageMapTile> tiles;
        if (!ImageMap::cut(image, gridColumns, gridRows, mode, tiles, error)) {
            fail(error);
            return 0;
        }

        const std::vector<int64_t> ids = ImageMapHandler::createMaps(owner(), *world, tiles);
        for (size_t index = 0; index < ids.size(); ++index)
            mapIds[index] = ids[index];
        gLastError.clear();
        return (uint32_t) ids.size();
    }

    int64_t mapCreateImage(FalconLevel *handle, const uint8_t *rgba, uint32_t length) {
        World *world = worldOf(handle);
        if (world == nullptr)
            return -1;

        ImageMapImage image;
        std::string error;
        if (!ImageMap::fromRgba(rgba, length, ImageMap::TILE_SIZE, ImageMap::TILE_SIZE, image, error)) {
            fail(error);
            return -1;
        }

        gLastError.clear();
        return ImageMapHandler::createMaps(owner(), *world, {image.mPixels}).front();
    }

    uint32_t mapCreateImageGrid(FalconLevel *handle, const uint8_t *rgba, uint32_t length, uint32_t width,
                                uint32_t height, uint32_t columns, uint32_t rows, FalconImageFit fit, int64_t *mapIds,
                                uint32_t capacity, uint32_t *outColumns, uint32_t *outRows) {
        ImageMapImage image;
        std::string error;
        if (width > (uint32_t) ImageMap::MAX_IMAGE_SIDE || height > (uint32_t) ImageMap::MAX_IMAGE_SIDE) {
            fail("images go up to " + std::to_string(ImageMap::MAX_IMAGE_SIDE) + " pixels a side");
            return 0;
        }
        if (!ImageMap::fromRgba(rgba, length, (int32_t) width, (int32_t) height, image, error)) {
            fail(error);
            return 0;
        }
        return createGrid(handle, image, columns, rows, fit, mapIds, capacity, outColumns, outRows);
    }

    uint32_t mapCreateImageGridFromPng(FalconLevel *handle, const uint8_t *png, uint32_t length, uint32_t columns,
                                       uint32_t rows, FalconImageFit fit, int64_t *mapIds, uint32_t capacity,
                                       uint32_t *outColumns, uint32_t *outRows) {
        ImageMapImage image;
        std::string error;
        if (!ImageMap::decodePng(png, length, image, error)) {
            fail(error);
            return 0;
        }
        return createGrid(handle, image, columns, rows, fit, mapIds, capacity, outColumns, outRows);
    }

    int mapPlaceImageGrid(FalconLevel *handle, FalconBlockPos topLeft, uint32_t facing, const int64_t *mapIds,
                          uint32_t count, uint32_t columns, uint32_t rows, int glowing) {
        if (handle == nullptr || mapIds == nullptr || columns == 0 || rows == 0
            || columns > (uint32_t) ImageMap::MAX_GRID_SIDE || rows > (uint32_t) ImageMap::MAX_GRID_SIDE) {
            fail("a grid needs a level, its ids and 1 to 32 maps a side");
            return 0;
        }
        // the caller's buffer holds count ids: never read past it
        if ((size_t) count != (size_t) columns * rows) {
            fail("a " + std::to_string(columns) + " x " + std::to_string(rows) + " grid needs "
                 + std::to_string(columns * rows) + " ids, not " + std::to_string(count));
            return 0;
        }

        const std::vector<int64_t> ids(mapIds, mapIds + count);
        std::string error;
        if (!ImageMapHandler::place(owner(), *level(handle), Vector3i(topLeft.x, topLeft.y, topLeft.z),
                                    (int32_t) facing, ids, (int32_t) columns, (int32_t) rows, glowing != 0, error)) {
            fail(error);
            return 0;
        }

        gLastError.clear();
        return 1;
    }

    int mapUpdateImage(FalconLevel *handle, int64_t mapId, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       const uint8_t *rgba, uint32_t length) {
        World *world = worldOf(handle);
        if (world == nullptr)
            return 0;
        if (width > (uint32_t) ImageMap::TILE_SIZE || height > (uint32_t) ImageMap::TILE_SIZE
            || x > (uint32_t) ImageMap::TILE_SIZE || y > (uint32_t) ImageMap::TILE_SIZE) {
            fail("the area must lie inside the 128 x 128 map");
            return 0;
        }

        ImageMapImage area;
        std::string error;
        if (!ImageMap::fromRgba(rgba, length, (int32_t) width, (int32_t) height, area, error)
            || !ImageMapHandler::update(owner(), *world, mapId, (int32_t) x, (int32_t) y, (int32_t) width,
                                        (int32_t) height, area.mPixels.data(), error)) {
            fail(error);
            return 0;
        }

        gLastError.clear();
        return 1;
    }

    int mapIsImage(FalconLevel *handle, int64_t mapId) {
        World *world = worldOf(handle);
        return world != nullptr && ImageMapHandler::isImageMap(*world, mapId) ? 1 : 0;
    }

    int playerGiveMap(FalconPlayer *target, int64_t mapId) {
        if (target == nullptr || !ImageMapHandler::give(owner(), *player(target), mapId)) {
            fail("map " + std::to_string(mapId) + " is not an image map of the player's world");
            return 0;
        }

        gLastError.clear();
        return 1;
    }

    int mapDeleteImage(FalconLevel *handle, int64_t mapId) {
        World *world = worldOf(handle);
        if (world == nullptr)
            return 0;

        std::string error;
        if (!ImageMapHandler::remove(owner(), *world, mapId, error)) {
            fail(error);
            return 0;
        }

        gLastError.clear();
        return 1;
    }

    const char *mapLastError() {
        return gLastError.c_str();
    }
}

void PluginServerApi::fillMaps(FalconServerApi &api) {
    api.mapCreateImage = &mapCreateImage;
    api.mapCreateImageGrid = &mapCreateImageGrid;
    api.mapCreateImageGridFromPng = &mapCreateImageGridFromPng;
    api.mapPlaceImageGrid = &mapPlaceImageGrid;
    api.mapUpdateImage = &mapUpdateImage;
    api.mapIsImage = &mapIsImage;
    api.playerGiveMap = &playerGiveMap;
    api.mapLastError = &mapLastError;
    api.mapDeleteImage = &mapDeleteImage;
}
