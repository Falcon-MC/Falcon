#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/PumpkinBlock.h"
#include "World/Level/Block/VanillaBlocks.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool facing(int32_t face, const char *direction) {
        return std::strcmp(PumpkinBlock::carvedDirection(face), direction) == 0;
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        require(dynamic_cast<const PumpkinBlock *>(VanillaBlocks::fromIdentifier("minecraft:pumpkin")) != nullptr,
                "a pumpkin is not a PumpkinBlock");
        require(dynamic_cast<const PumpkinBlock *>(VanillaBlocks::fromIdentifier("minecraft:carved_pumpkin"))
                        == nullptr,
                "a carved pumpkin can be carved again");
        require(facing(2, "north") && facing(3, "south") && facing(4, "west") && facing(5, "east"),
                "a side face did not carve toward itself");
        require(facing(0, "south") && facing(1, "south"), "the top or bottom did not keep the default");
    } catch (const std::exception &error) {
        std::clog << "Pumpkin carving regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
