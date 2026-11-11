#pragma once

#include <string>
#include <vector>

class BlockState;
class Item;

// the Item and Block command enums: the game leaves some names out, a command naming one does not parse
// (BedrockData's command_hidden_items.json and command_hidden_blocks.json)
namespace CommandEnums {
    std::vector<std::string> itemValues();

    // false for an unknown item and for one the enum leaves out
    bool parseItem(const std::string &input, Item &out);

    std::vector<std::string> blockValues();

    // false for an unknown block and for one the enum leaves out
    bool parseBlock(const std::string &input, BlockState &out);
}
