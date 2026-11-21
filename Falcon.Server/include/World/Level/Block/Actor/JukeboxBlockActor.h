#pragma once

#include "World/Inventory/Container.h"
#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>
#include <string>

class ServerNetworkHandler;

/**
 * The block actor of a jukebox, and its one-slot container: a hopper may put a record in, which then plays, and
 * take it out once it has finished. Its record plays for the song's length, powers the jukebox meanwhile and gives
 * comparators the song's signal.
 */
class JukeboxBlockActor final : public BlockActor, public Container {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Jukebox";

    struct Song {
        // the music_disc_ item name after its prefix, and the record_ language keys
        const char *mDisc;
        int32_t mComparatorSignal;
        float mSeconds;
    };

    // the song of a record item, null for any other item
    static const Song *findSong(const ItemStack &record);

    static std::string soundOf(const Song &song);

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // the client starts or stops the disc from FinishedRecording
    Tag getSpawnCompound() const override;

    Container *getContainer() override {
        return this;
    }

    bool tick(ServerNetworkHandler &owner) override;

    // the record drops out, whatever removed the block
    void onRemoved() override;

    int getContainerSize() const override {
        return 1;
    }

    const ItemStack &getContainerItem(int slot) const override;

    // what a hopper puts in plays
    void setContainerItem(int slot, ItemStack item) override;

    bool canHoldInSlot(int slot, const ItemStack &item) const override;

    // a hopper takes the record only once it has finished
    bool canTakeFromSlot(int slot) const override;

    const ItemStack &getRecord() const {
        return mRecord;
    }

    bool isPlaying() const {
        return !mFinishedRecording;
    }

    // a single record; the sound and the redstone follow on the next tick
    void setRecord(ItemStack record, bool play);

    void play(ServerNetworkHandler &owner);

    void stop(ServerNetworkHandler &owner);

    // the record drops on the first free side, starting above
    void eject(ServerNetworkHandler &owner);

    int32_t getComparatorSignal() const;

private:
    enum class Pending : uint8_t { None, Play, Stop };

    void _changed(ServerNetworkHandler &owner);

    void _drop(ServerNetworkHandler &owner);

    ItemStack mRecord = ItemStack::air();
    bool mFinishedRecording = true;
    // the client's copy: the game never counts it up on the server
    int32_t mTicksPlaying = 0;
    int32_t mPlayedTicks = 0;
    Pending mPending = Pending::None;
};
