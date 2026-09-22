#pragma once

#include "Protocol/Types/StartGameTypes.h"

#include <vector>

class DataDrivenBlockDefinitions {
public:
    static const std::vector<BlockPropertyData> &getAll();
};
