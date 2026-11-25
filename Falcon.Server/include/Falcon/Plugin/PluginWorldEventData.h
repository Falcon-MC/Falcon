#pragma once

#include <string>

class BlockState;

namespace json {
    struct Value;
}

namespace PluginWorldEventData {
    std::string encodeBlock(const BlockState &state);
    bool decodeBlock(const json::Value &value, BlockState &state);
    bool decodeBlock(const std::string &payload, BlockState &state);
}
