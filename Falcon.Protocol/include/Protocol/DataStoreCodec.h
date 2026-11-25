#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/Types/DataStoreTypes.h"

class DataStoreCodec {
public:
    static void writeScalar(BinaryStream &stream, const DataStoreScalar &value);

    static DataStoreScalar readScalar(ReadOnlyBinaryStream &stream);

    static void writeChangeValue(BinaryStream &stream, const DataStoreChangeValue &value);

    static DataStoreChangeValue readChangeValue(ReadOnlyBinaryStream &stream, int depth = 0);

    static void writeUpdate(BinaryStream &stream, const DataStoreUpdate &update, bool variables);

    static DataStoreUpdate readUpdate(ReadOnlyBinaryStream &stream, bool variables);

    static void writeAction(BinaryStream &stream, const DataStoreAction &action, bool variables);

    static DataStoreAction readAction(ReadOnlyBinaryStream &stream, bool variables);
};
