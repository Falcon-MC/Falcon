#pragma once

#include "Protocol/Packets/TrimDataPacket.h"

/** Armor trim patterns and materials the client needs to render trimmed armor and fill the smithing table. */
class ArmorTrims {
public:
    static TrimDataPacket getPacket();
};
