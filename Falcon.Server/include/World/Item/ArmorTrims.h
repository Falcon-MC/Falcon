#pragma once

#include "Protocol/Packets/TrimDataPacket.h"

// the client needs them to render trimmed armor and fill the smithing table
class ArmorTrims {
public:
    static TrimDataPacket getPacket();
};
