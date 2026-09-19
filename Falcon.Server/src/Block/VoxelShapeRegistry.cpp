#include "Block/VoxelShapeRegistry.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "VoxelShapesJson.h"

#include <array>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    struct VoxelBox {
        std::array<float, 3> mMin;
        std::array<float, 3> mMax;
    };

    SerializableVoxelShape createEmptyShape() {
        SerializableVoxelShape shape;
        shape.mXCoordinates.push_back(0.0f);
        shape.mYCoordinates.push_back(0.0f);
        shape.mZCoordinates.push_back(0.0f);
        return shape;
    }

    std::vector<float> getAxisBoundaries(const std::vector<VoxelBox> &boxes, size_t axis) {
        std::set<float> bounds;
        for (const VoxelBox &box : boxes) {
            bounds.insert(box.mMin[axis]);
            bounds.insert(box.mMax[axis]);
        }
        return std::vector<float>(bounds.begin(), bounds.end());
    }

    bool isInside(float x, float y, float z, const std::vector<VoxelBox> &boxes) {
        for (const VoxelBox &box : boxes) {
            if (x >= box.mMin[0] && x <= box.mMax[0] &&
                y >= box.mMin[1] && y <= box.mMax[1] &&
                z >= box.mMin[2] && z <= box.mMax[2])
                return true;
        }
        return false;
    }

    SerializableVoxelShape convertBoxesToShape(const std::vector<VoxelBox> &boxes) {
        if (boxes.empty())
            return createEmptyShape();

        const std::vector<float> xCoordinates = getAxisBoundaries(boxes, 0);
        const std::vector<float> yCoordinates = getAxisBoundaries(boxes, 1);
        const std::vector<float> zCoordinates = getAxisBoundaries(boxes, 2);

        const size_t resolutionX = xCoordinates.size() - 1;
        const size_t resolutionY = yCoordinates.size() - 1;
        const size_t resolutionZ = zCoordinates.size() - 1;

        SerializableVoxelShape shape;
        shape.mCells.mXSize = (uint8_t) resolutionX;
        shape.mCells.mYSize = (uint8_t) resolutionY;
        shape.mCells.mZSize = (uint8_t) resolutionZ;
        shape.mCells.mStorage.assign((resolutionX * resolutionY * resolutionZ + 7) / 8, 0);

        for (size_t z = 0; z < resolutionZ; ++z) {
            for (size_t y = 0; y < resolutionY; ++y) {
                for (size_t x = 0; x < resolutionX; ++x) {
                    const float middleX = (xCoordinates[x] + xCoordinates[x + 1]) / 2.0f;
                    const float middleY = (yCoordinates[y] + yCoordinates[y + 1]) / 2.0f;
                    const float middleZ = (zCoordinates[z] + zCoordinates[z + 1]) / 2.0f;

                    if (!isInside(middleX, middleY, middleZ, boxes))
                        continue;

                    const size_t bit = z + y * resolutionZ + x * resolutionZ * resolutionY;
                    shape.mCells.mStorage[bit / 8] |= (uint8_t) (1u << (bit % 8));
                }
            }
        }

        shape.mXCoordinates = xCoordinates;
        shape.mYCoordinates = yCoordinates;
        shape.mZCoordinates = zCoordinates;
        return shape;
    }

    bool readCorner(const json::Value &value, std::array<float, 3> &out) {
        if (value.mType != json::Value::Type::Array || value.mArray.size() != 3)
            return false;

        for (size_t axis = 0; axis < 3; ++axis) {
            if (value.mArray[axis]->mType != json::Value::Type::Number)
                return false;
            out[axis] = (float) value.mArray[axis]->mNumber / 16.0f;
        }
        return true;
    }

    std::vector<VoxelBox> readBoxes(const json::Value &entry) {
        std::vector<VoxelBox> boxes;
        const json::Value *list = entry.get("boxes");
        if (list == nullptr || list->mType != json::Value::Type::Array)
            return boxes;

        for (const std::unique_ptr<json::Value> &box : list->mArray) {
            if (box->mType != json::Value::Type::Array || box->mArray.size() != 2)
                continue;

            VoxelBox voxelBox;
            if (readCorner(*box->mArray[0], voxelBox.mMin) && readCorner(*box->mArray[1], voxelBox.mMax))
                boxes.push_back(voxelBox);
        }
        return boxes;
    }

    VoxelShapesPacket buildPacket() {
        VoxelShapesPacket packet;

        const std::string source(FalconVoxelShapeData::kVoxelShapesJson);
        const std::unique_ptr<json::Value> root = json::parse(source);
        if (root == nullptr || root->mType != json::Value::Type::Array) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded voxel shapes");
            return packet;
        }

        std::unordered_set<std::string> names;
        std::vector<SerializableVoxelShape> anonymousShapes;

        for (const std::unique_ptr<json::Value> &entry : root->mArray) {
            if (entry->mType != json::Value::Type::Object)
                continue;

            SerializableVoxelShape shape = convertBoxesToShape(readBoxes(*entry));
            const json::Value *identifier = entry->get("identifier");
            if (identifier == nullptr || identifier->mType != json::Value::Type::String) {
                anonymousShapes.push_back(std::move(shape));
                continue;
            }

            if (!names.insert(identifier->mString).second)
                continue;

            packet.mNameMap.emplace_back(identifier->mString, (uint16_t) packet.mShapes.size());
            packet.mShapes.push_back(std::move(shape));
        }

        for (SerializableVoxelShape &shape : anonymousShapes)
            packet.mShapes.push_back(std::move(shape));

        packet.mCustomShapeCount = 0;
        return packet;
    }
}

const VoxelShapesPacket &VoxelShapeRegistry::getPacket() {
    static const VoxelShapesPacket packet = buildPacket();
    return packet;
}
