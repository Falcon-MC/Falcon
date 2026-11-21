#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <string>

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

class NoteBlock final : public Block {
public:
    explicit NoteBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    // the instrument of a note.* sound, as minecraft:instrument_sound names them
    static bool instrumentOfSound(const std::string &sound, NoteBlockInstrument &instrument);

    // what a note block at `position` plays; false when the block above silences it
    static bool instrumentAt(Level &level, const Vector3i &position, NoteBlockInstrument &instrument);

    const char *getBlockActorId() const override;

    // steps the pitch, then plays it
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // plays the current pitch; breaking goes on
    bool onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                 const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;

    // plays on a rising edge of power
    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;
};
