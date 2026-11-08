#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Map/MapItemSavedData.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// how an image meets a grid of another aspect ratio
enum class ImageMapFit : uint8_t {
    Stretch = 0,
    // the whole image, centred; the margins stay transparent
    Contain = 1,
    // the grid fully covered, centred; the overflow is cropped
    Cover = 2,
};

// one uint32_t per pixel, red in the low byte: the layout of MapItemSavedData and of the client texture
struct ImageMapImage {
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::vector<uint32_t> mPixels;
};

using ImageMapTile = std::vector<uint32_t>;

// pictures cut into 128 x 128 locked maps; pure functions, the server side lives in ImageMapHandler
namespace ImageMap {
    const int32_t TILE_SIZE = MapItemSavedData::MAP_SIZE;
    const int32_t MAX_GRID_SIDE = 32;
    const int32_t MAX_IMAGE_SIDE = 4096;
    const size_t MAX_PNG_SIZE = 32u * 1024u * 1024u;

    uint32_t pack(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

    // rgba holds width * height * 4 bytes, rows top to bottom
    bool fromRgba(const uint8_t *rgba, size_t length, int32_t width, int32_t height, ImageMapImage &out,
                  std::string &error);

    bool decodePng(const uint8_t *data, size_t length, ImageMapImage &out, std::string &error);

    // 0 picks the side: both 0 keep the image size, one 0 keeps its aspect ratio
    bool gridSize(int32_t width, int32_t height, int32_t &columns, int32_t &rows, std::string &error);

    ImageMapImage fitTo(const ImageMapImage &image, int32_t width, int32_t height, ImageMapFit fit);

    // image must be columns * 128 by rows * 128; tiles row by row from the top left
    std::vector<ImageMapTile> split(const ImageMapImage &image, int32_t columns, int32_t rows);

    // gridSize, fitTo then split; columns and rows come back resolved
    bool cut(const ImageMapImage &image, int32_t &columns, int32_t &rows, ImageMapFit fit,
             std::vector<ImageMapTile> &tiles, std::string &error);

    // frame facing_direction 2 to 5; the tile at column c, row r sits at out[r * columns + c], read from the front
    bool wallPositions(const Vector3i &topLeft, int32_t facing, int32_t columns, int32_t rows,
                       std::vector<Vector3i> &out);

    // the block a frame with this facing hangs on
    Vector3i supportOf(const Vector3i &position, int32_t facing);

    // one step to the viewer's right on a wall with this facing
    Vector3i rightOf(int32_t facing);
}
