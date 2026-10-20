#pragma once

#include "Protocol/Packets/VoxelShapesPacket.h"

class VoxelShapeRegistry {
public:
    static const VoxelShapesPacket &getPacket();
};
