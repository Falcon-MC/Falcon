#pragma once

#include <cstdint>
#include <vector>

class SerializableVoxelShapeCells {
public:
    uint8_t mXSize = 0;
    uint8_t mYSize = 0;
    uint8_t mZSize = 0;
    std::vector<uint8_t> mStorage;
};

class SerializableVoxelShape {
public:
    SerializableVoxelShapeCells mCells;
    std::vector<float> mXCoordinates;
    std::vector<float> mYCoordinates;
    std::vector<float> mZCoordinates;
};
