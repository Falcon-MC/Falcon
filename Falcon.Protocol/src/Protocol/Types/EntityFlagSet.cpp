#include "Protocol/Types/EntityFlagSet.h"

void EntityFlagCodec::writeFlags(BinaryStream &stream, const std::vector<bool> &flags) {
    size_t total = flags.size();
    size_t position = 0;

    do {
        unsigned char byte = 0;
        for (int bit = 0; bit < 7; bit++) {
            size_t index = position + bit;
            if (index < total && flags[index])
                byte |= (unsigned char) (1 << bit);
        }

        position += 7;

        bool more = false;
        for (size_t index = position; index < total; index++) {
            if (flags[index]) {
                more = true;
                break;
            }
        }

        stream.putByte(more ? (byte | 0x80) : byte);
    } while (position < total);
}

std::vector<bool> EntityFlagCodec::readFlags(ReadOnlyBinaryStream &stream) {
    std::vector<bool> flags;
    size_t position = 0;

    while (true) {
        unsigned char byte = stream.getByte();

        for (int bit = 0; bit < 7; bit++) {
            size_t index = position + bit;
            if (flags.size() <= index)
                flags.resize(index + 1, false);
            flags[index] = (byte & (1 << bit)) != 0;
        }

        position += 7;

        if ((byte & 0x80) == 0)
            break;
    }

    return flags;
}
