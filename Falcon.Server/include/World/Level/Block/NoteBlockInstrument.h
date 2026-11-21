#pragma once

#include <cstdint>

class Block;

// NoteBlockInstrument, in the game's order: the event type of the block event a note block sends
enum class NoteBlockInstrument : uint8_t {
    Harp,
    BassDrum,
    Snare,
    Hat,
    Bass,
    Flute,
    Bell,
    Guitar,
    Chime,
    Xylophone,
    IronXylophone,
    CowBell,
    Didgeridoo,
    Bit,
    Banjo,
    Pling,
    Trumpet,
    TrumpetExposed,
    TrumpetWeathered,
    TrumpetOxidized,
    Zombie,
    Skeleton,
    Creeper,
    EnderDragon,
    WitherSkeleton,
    Piglin,
    None
};

namespace NoteBlockInstruments {
    // gives a vanilla block the minecraft:instrument_sound faces the game registers for it
    void assign(Block &block);
}
