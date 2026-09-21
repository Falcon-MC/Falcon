#pragma once

#include <cstddef>
#include <cstdint>

struct CreativeGroupEntry {
    uint8_t mCategory;
    const char *mName;
    const char *mIconIdentifier;
};

struct CreativeEntry {
    const char *mIdentifier;
    int32_t mGroupIndex;
    int32_t mDamage;
    const unsigned char *mNbt;
    size_t mNbtSize;
};

class CreativeContentTable {
public:
    static const CreativeGroupEntry *getGroups();

    static size_t getGroupCount();

    static const CreativeEntry *getEntries();

    static size_t getEntryCount();
};
