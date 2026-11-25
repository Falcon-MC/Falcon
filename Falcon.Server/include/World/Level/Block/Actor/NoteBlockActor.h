#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class NoteBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Music";

    // the 25 pitches a note block steps through, from F#3 to F#5
    static constexpr int32_t PITCH_COUNT = 25;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    int32_t getNote() const noexcept {
        return mNote;
    }

    void stepNote() {
        mNote = (mNote + 1) % PITCH_COUNT;
    }

    bool isPowered() const noexcept {
        return mPowered;
    }

    void setPowered(bool powered) {
        mPowered = powered;
    }

private:
    int32_t mNote = 0;
    bool mPowered = false;
};
