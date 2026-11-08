#include "Core/NBT/Tag.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Map/ImageMap.h"
#include "World/Level/Map/MapItemSavedData.h"
#include "World/Level/Map/MapRenderer.h"

#include <zlib.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a different colour for every pixel, so a tile shows where it was cut from
    ImageMapImage gradient(int32_t width, int32_t height) {
        ImageMapImage image;
        image.mWidth = width;
        image.mHeight = height;
        image.mPixels.resize((size_t) width * (size_t) height);
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x)
                image.mPixels[(size_t) (x + y * width)] = ImageMap::pack((uint8_t) x, (uint8_t) y,
                                                                         (uint8_t) (x >> 8), 255);
        }
        return image;
    }

    void appendChunk(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &data) {
        const uint32_t length = (uint32_t) data.size();
        for (int shift = 24; shift >= 0; shift -= 8)
            png.push_back((uint8_t) (length >> shift));

        std::vector<uint8_t> body(type, type + 4);
        body.insert(body.end(), data.begin(), data.end());
        png.insert(png.end(), body.begin(), body.end());
        const uint32_t crc = (uint32_t) crc32(0, body.data(), (uInt) body.size());
        for (int shift = 24; shift >= 0; shift -= 8)
            png.push_back((uint8_t) (crc >> shift));
    }

    std::vector<uint8_t> header(uint32_t width, uint32_t height) {
        std::vector<uint8_t> data;
        for (const uint32_t value: {width, height}) {
            for (int shift = 24; shift >= 0; shift -= 8)
                data.push_back((uint8_t) (value >> shift));
        }
        data.push_back(8);
        data.push_back(6);
        data.push_back(0);
        data.push_back(0);
        data.push_back(0);
        return data;
    }

    // rgba, 8 bits, no filter
    std::vector<uint8_t> encodePng(uint32_t width, uint32_t height, const std::vector<uint8_t> &rgba) {
        std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        appendChunk(png, "IHDR", header(width, height));

        std::vector<uint8_t> raw;
        for (uint32_t y = 0; y < height; ++y) {
            raw.push_back(0);
            raw.insert(raw.end(), rgba.begin() + (ptrdiff_t) (y * width * 4),
                       rgba.begin() + (ptrdiff_t) ((y + 1) * width * 4));
        }
        uLongf size = compressBound((uLong) raw.size());
        std::vector<uint8_t> packed(size);
        compress(packed.data(), &size, raw.data(), (uLong) raw.size());
        packed.resize(size);
        appendChunk(png, "IDAT", packed);
        appendChunk(png, "IEND", {});
        return png;
    }

    class StoneSource final : public MapRenderSource {
    public:
        bool hasChunk(int32_t chunkX, int32_t chunkZ) override {
            (void) chunkX;
            (void) chunkZ;
            return true;
        }

        int32_t getSurfaceY(int32_t x, int32_t z) override {
            (void) x;
            (void) z;
            return 65;
        }

        const BlockState &getBlock(int32_t x, int32_t y, int32_t z) override {
            (void) x;
            (void) z;
            return y <= 64 ? mStone : mAir;
        }

        MapBiomeColors getBiome(int32_t x, int32_t y, int32_t z) override {
            (void) x;
            (void) y;
            (void) z;
            return MapBiomeColors();
        }

        int32_t getMinY() const override {
            return -64;
        }

        int32_t getMaxY() const override {
            return 320;
        }

        bool hasCeiling() const override {
            return false;
        }

    private:
        BlockState mStone = BlockState("minecraft:stone");
        BlockState mAir;
    };

    void gridCutting() {
        const ImageMapImage image = gradient(256, 128);
        int32_t columns = 0;
        int32_t rows = 0;
        std::vector<ImageMapTile> tiles;
        std::string error;
        require(ImageMap::cut(image, columns, rows, ImageMapFit::Contain, tiles, error), "a 256 x 128 image cuts");
        require(columns == 2 && rows == 1 && tiles.size() == 2, "two maps side by side");
        require(tiles[0][0] == image.mPixels[0] && tiles[1][0] == image.mPixels[128],
                "the second tile starts at column 128");
        require(tiles[1][127 + 127 * 128] == image.mPixels[255 + 127 * 256], "the last pixel lands bottom right");

        const ImageMapImage tall = gradient(256, 640);
        columns = 0;
        rows = 0;
        require(ImageMap::cut(tall, columns, rows, ImageMapFit::Contain, tiles, error), "a tall image cuts");
        require(columns == 2 && rows == 5 && tiles.size() == 10, "row major, five rows of two");
        require(tiles[3][0] == tall.mPixels[128 + 128 * 256], "tile 3 is row 1, column 1");

        columns = 4;
        rows = 0;
        require(ImageMap::gridSize(256, 128, columns, rows, error) && rows == 2, "one side keeps the aspect ratio");
        columns = 0;
        rows = 3;
        require(ImageMap::gridSize(256, 128, columns, rows, error) && columns == 6, "either side");
    }

    void fitting() {
        ImageMapImage square;
        square.mWidth = 2;
        square.mHeight = 2;
        square.mPixels.assign(4, ImageMap::pack(255, 0, 0, 255));

        const ImageMapImage contained = ImageMap::fitTo(square, 256, 128, ImageMapFit::Contain);
        require(contained.mPixels[0] == 0 && contained.mPixels[255] == 0, "fit leaves the sides transparent");
        require(contained.mPixels[128 + 64 * 256] == square.mPixels[0], "fit keeps the image in the middle");

        ImageMapImage wide = gradient(400, 100);
        const ImageMapImage covered = ImageMap::fitTo(wide, 128, 128, ImageMapFit::Cover);
        require(covered.mPixels[0] != 0 && covered.mPixels[127 + 127 * 128] != 0, "fill covers every pixel");
        require((covered.mPixels[0] & 0xff) > 100, "fill crops the left of a wide image");

        const ImageMapImage stretched = ImageMap::fitTo(wide, 128, 128, ImageMapFit::Stretch);
        require((stretched.mPixels[0] & 0xff) < 4 && (stretched.mPixels[127] & 0xff) > 140,
                "stretch keeps both edges");
    }

    void wallOrder() {
        std::vector<Vector3i> positions;
        const Vector3i origin(10, 70, -5);
        require(ImageMap::wallPositions(origin, 3, 3, 2, positions) && positions.size() == 6, "a 3 x 2 wall");
        require(positions[0] == origin, "the first map is the top left");
        require(positions[1] == Vector3i(11, 70, -5) && positions[3] == Vector3i(10, 69, -5),
                "facing south: right is east, rows go down");

        ImageMap::wallPositions(origin, 2, 2, 1, positions);
        require(positions[1] == Vector3i(9, 70, -5), "facing north: right is west");
        ImageMap::wallPositions(origin, 4, 2, 1, positions);
        require(positions[1] == Vector3i(10, 70, -4), "facing west: right is south");
        ImageMap::wallPositions(origin, 5, 2, 1, positions);
        require(positions[1] == Vector3i(10, 70, -6), "facing east: right is north");

        require(ImageMap::supportOf(origin, 3) == Vector3i(10, 70, -6), "a south frame hangs on the block north");
        require(ImageMap::supportOf(origin, 4) == Vector3i(11, 70, -5), "a west frame hangs on the block east");
        require(!ImageMap::wallPositions(origin, 1, 1, 1, positions), "floors and ceilings are refused");
        require(!ImageMap::wallPositions(origin, 3, 33, 1, positions), "walls stop at 32 maps a side");
    }

    void neverRendered() {
        MapItemSavedData map(5);
        map.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        map.setLocked(true);
        map.setImage(true);
        std::vector<uint32_t> blank((size_t) MapItemSavedData::PIXEL_COUNT, 0);
        MapPixelArea changed;
        map.writePixels(0, 0, 128, 128, blank.data(), changed);

        StoneSource stone;
        MapRenderer::update(map, stone, Vector3i(0, 64, 0));
        require(map.getPixel(64, 64) == 0 && map.getPixel(0, 0) == 0, "the world never draws on an image map");

        std::vector<MapPixel> pixels(1);
        pixels[0].mIndex = 10;
        pixels[0].mPixel = (int32_t) 0xff112233u;
        require(!map.applyClientPixels(pixels) && map.getPixel(10, 0) == 0, "client pixels are refused");

        MapItemSavedData plain(6);
        plain.setOrigin(Vector3f(0.0f, 0.0f, 0.0f), 0, 0);
        MapRenderer::update(plain, stone, Vector3i(0, 64, 0));
        require(plain.getPixel(64, 64) != 0, "a normal map still renders");

        Tag tag = Tag::ofCompound();
        map.save(tag);
        MapItemSavedData loaded(5);
        loaded.load(tag);
        require(loaded.isImage() && loaded.isLocked(), "the image flag survives a save");

        MapItemSavedData copy(7);
        copy.copyFrom(loaded);
        require(copy.isImage(), "a cartography copy stays an image");
    }

    void partialUpdate() {
        MapItemSavedData map(8);
        map.setImage(true);
        std::vector<uint32_t> block(6, ImageMap::pack(1, 2, 3, 255));
        MapPixelArea changed;
        require(map.writePixels(20, 30, 3, 2, block.data(), changed), "a block write changes pixels");
        require(changed.mMinX == 20 && changed.mMaxX == 22 && changed.mMinY == 30 && changed.mMaxY == 31,
                "the changed area is the block");
        require(map.isDirtyForSave(), "a write is saved");

        block[0] = ImageMap::pack(9, 9, 9, 255);
        require(map.writePixels(20, 30, 3, 2, block.data(), changed), "one pixel differs");
        require(changed.mMinX == 20 && changed.mMaxX == 20 && changed.mMinY == 30 && changed.mMaxY == 30,
                "only the pixel that changed");

        const ClientboundMapItemDataPacket packet = map.buildAreaPacket(changed);
        require(packet.mXOffset == 20 && packet.mYOffset == 30 && packet.mWidth == 1 && packet.mHeight == 1,
                "the packet covers the changed area only");
        require(packet.mColors.size() == 1 && (uint32_t) packet.mColors[0] == block[0], "with its colour");

        require(!map.writePixels(20, 30, 3, 2, block.data(), changed) && changed.isEmpty(),
                "the same pixels change nothing");
        require(!map.writePixels(127, 0, 2, 1, block.data(), changed), "an area out of the map is refused");
        require(!map.writePixels(-1, 0, 1, 1, block.data(), changed), "negative offsets are refused");
    }

    void limits() {
        std::string error;
        int32_t columns = 33;
        int32_t rows = 1;
        require(!ImageMap::gridSize(4096, 128, columns, rows, error), "33 maps across are refused");
        columns = 0;
        rows = 0;
        require(ImageMap::gridSize(4096, 4096, columns, rows, error) && columns == 32 && rows == 32,
                "the largest image is 32 x 32 maps");
        columns = 0;
        rows = 40;
        require(!ImageMap::gridSize(10, 10, columns, rows, error), "a derived side over 32 is refused");

        ImageMapImage image;
        std::vector<uint8_t> rgba(4097 * 4, 255);
        require(!ImageMap::fromRgba(rgba.data(), rgba.size(), 4097, 1, image, error), "4097 pixels wide is refused");
        require(!ImageMap::fromRgba(rgba.data(), 7, 1, 1, image, error), "a short buffer is refused");

        std::vector<uint8_t> huge = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        appendChunk(huge, "IHDR", header(20000, 20000));
        appendChunk(huge, "IEND", {});
        require(!ImageMap::decodePng(huge.data(), huge.size(), image, error), "a 20000 pixel png is refused");

        std::vector<uint8_t> oversized = encodePng(1, 1, std::vector<uint8_t>(4, 255));
        oversized.resize(ImageMap::MAX_PNG_SIZE + 1, 0);
        require(!ImageMap::decodePng(oversized.data(), oversized.size(), image, error), "a png over 32 MB is refused");

        const std::vector<uint8_t> garbage(64, 7);
        require(!ImageMap::decodePng(garbage.data(), garbage.size(), image, error), "garbage is refused");

        const std::vector<uint8_t> pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 1, 2, 3, 0};
        const std::vector<uint8_t> small = encodePng(2, 2, pixels);
        require(ImageMap::decodePng(small.data(), small.size(), image, error), "a real png decodes");
        require(image.mWidth == 2 && image.mPixels[0] == ImageMap::pack(255, 0, 0, 255)
                && image.mPixels[3] == ImageMap::pack(1, 2, 3, 0), "rgba comes through, alpha included");
    }
}

int main() {
    try {
        gridCutting();
        fitting();
        wallOrder();
        neverRendered();
        partialUpdate();
        limits();
        std::cout << "image map cutting, walls, rendering, updates and limits checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
