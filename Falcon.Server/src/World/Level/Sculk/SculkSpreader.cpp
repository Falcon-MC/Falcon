#include "World/Level/Sculk/SculkSpreader.h"

#include <algorithm>

namespace {
    const char *TAG_CURSORS = "cursors";
}

// the delays and the facing are shorts on disk
Tag SculkChargeCursor::save() const {
    Tag data = Tag::ofCompound();
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    data.putShort("charge", (int16_t) mCharge);
    data.putShort("update", (int16_t) mUpdateDelay);
    data.putShort("decay", (int16_t) mDecayDelay);
    data.putShort("facing", (int16_t) mFacing);
    return data;
}

SculkChargeCursor SculkChargeCursor::load(const Tag &data) {
    SculkChargeCursor cursor;
    cursor.mPosition = Vector3i(data.getInt("x"), data.getInt("y"), data.getInt("z"));
    cursor.mCharge = data.getShort("charge");
    cursor.mUpdateDelay = data.getShort("update");
    cursor.mDecayDelay = data.getShort("decay");
    cursor.mFacing = data.getShort("facing");
    return cursor;
}

void SculkSpreader::addCursors(const Vector3i &position, int32_t charge) {
    while (charge > 0) {
        const int32_t part = std::min(charge, MAX_CHARGE);
        if (mCursors.size() < MAX_CURSORS) {
            SculkChargeCursor cursor;
            cursor.mPosition = position;
            cursor.mCharge = part;
            mCursors.push_back(cursor);
        }
        charge -= part;
    }
}

int32_t SculkSpreader::getTotalCharge() const {
    int32_t total = 0;
    for (const SculkChargeCursor &cursor: mCursors)
        total += cursor.mCharge;
    return total;
}

void SculkSpreader::save(Tag &data) const {
    if (mCursors.empty())
        return;

    Tag cursors = Tag::ofList(Tag::Type::Compound);
    for (const SculkChargeCursor &cursor: mCursors)
        cursors.addToList(cursor.save());
    data.put(TAG_CURSORS, cursors);
}

void SculkSpreader::load(const Tag &data) {
    if (!mCursors.empty())
        return;

    const Tag *cursors = data.get(TAG_CURSORS);
    if (cursors == nullptr || !cursors->isList())
        return;

    for (const Tag &entry: cursors->getList()) {
        if (entry.isCompound() && mCursors.size() < MAX_CURSORS)
            mCursors.push_back(SculkChargeCursor::load(entry));
    }
}
