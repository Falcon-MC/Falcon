#pragma once

#include "World/Level/Block/BlockState.h"

#include <optional>

namespace json { struct Value; }

namespace PackBlockState {
    std::optional<BlockState> read(const json::Value &value);
}
