#include "World/Level/Block/NoteBlockInstrument.h"

#include "World/Level/Block/Block.h"

#include <iterator>
#include <string>
#include <unordered_map>

namespace {
    struct Entry {
        const char *mIdentifier;
        NoteBlockInstrument mInstrument;
    };

#include "NoteBlockInstruments.inc"

    // the up face of the mob heads; the player head has none
    const Entry ABOVE[] = {
            {"minecraft:zombie_head", NoteBlockInstrument::Zombie},
            {"minecraft:skeleton_skull", NoteBlockInstrument::Skeleton},
            {"minecraft:creeper_head", NoteBlockInstrument::Creeper},
            {"minecraft:dragon_head", NoteBlockInstrument::EnderDragon},
            {"minecraft:wither_skeleton_skull", NoteBlockInstrument::WitherSkeleton},
            {"minecraft:piglin_head", NoteBlockInstrument::Piglin},
    };

    using Table = std::unordered_map<std::string, NoteBlockInstrument>;

    Table tableOf(const Entry *begin, const Entry *end) {
        Table table;
        for (const Entry *entry = begin; entry != end; ++entry)
            table.emplace(entry->mIdentifier, entry->mInstrument);
        return table;
    }
}

void NoteBlockInstruments::assign(Block &block) {
    static const Table below = tableOf(std::begin(BELOW), std::end(BELOW));
    static const Table above = tableOf(std::begin(ABOVE), std::end(ABOVE));

    const auto down = below.find(block.getIdentifier());
    const auto up = above.find(block.getIdentifier());
    block.setInstrumentSounds(down == below.end() ? NoteBlockInstrument::Harp : down->second,
                              up == above.end() ? nullptr : &up->second);
}
