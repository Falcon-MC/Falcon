#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"

#include <functional>

using MaterialRule = std::function<const BlockState *(FunctionContext &)>;
