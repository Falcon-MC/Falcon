#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

// both faces of a sign, kept as the game saves them; an unwritten face has empty text in opaque black
class SignBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Sign";

    // hanging signs share the layout under their own id
    static constexpr const char *HANGING_ID = "HangingSign";

    explicit SignBlockActor(bool hanging = false) : mHanging(hanging) {}

    const char *getBlockActorId() const override {
        return mHanging ? HANGING_ID : BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool isWaxed() const {
        return mWaxed;
    }

    void setWaxed(bool waxed) {
        mWaxed = waxed;
    }

    Tag &getSide(bool front) {
        return front ? mFront : mBack;
    }

    // the editing player's unique id, -1 when nobody edits; kept in memory only
    int64_t getEditor() const {
        return mEditor;
    }

    void setEditor(int64_t editor) {
        mEditor = editor;
    }

private:
    void _putFields(Tag &data) const;

    bool mHanging;
    Tag mFront = emptyText();
    Tag mBack = emptyText();
    bool mWaxed = false;
    int64_t mEditor = -1;

    static Tag emptyText();
};
