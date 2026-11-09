#pragma once

#include <string>
#include <vector>

class Item;

// the Item command enum: the game leaves some items out, a command naming one does not parse
// (BedrockData's command_hidden_items.json)
namespace CommandItems {
    std::vector<std::string> enumValues();

    // false for an unknown item and for one the enum leaves out
    bool parse(const std::string &input, Item &out);
}
