#include "Protocol/DataStoreCodec.h"

#include "Core/Utility/BinaryDataException.h"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

int main() {
    try {
        // a change value of type map (6) nests another change value; a chain deeper than the limit must be
        // rejected with an exception rather than recursing until the stack overflows.
        BinaryStream wire;
        const int depth = 200;
        for (int i = 0; i < depth; ++i) {
            wire.putLInt(6);              // DataStoreChangeValueType::Map
            wire.putUnsignedVarInt(1);    // one entry
            wire.putString("k");          // its key
        }
        wire.putLInt(0);                  // innermost: Null

        ReadOnlyBinaryStream input(wire.getBuffer());
        bool threw = false;
        try {
            DataStoreCodec::readChangeValue(input);
        } catch (const BinaryDataException &) {
            threw = true;
        }
        require(threw, "deeply nested data store map was not rejected");

        // a shallow map still decodes
        BinaryStream shallow;
        shallow.putLInt(6);
        shallow.putUnsignedVarInt(1);
        shallow.putString("k");
        shallow.putLInt(1);               // Bool
        shallow.putBool(true);
        ReadOnlyBinaryStream shallowInput(shallow.getBuffer());
        const DataStoreChangeValue value = DataStoreCodec::readChangeValue(shallowInput);
        require(value.mType == DataStoreChangeValueType::Map, "shallow map lost its type");
        require(value.mMapValue.size() == 1, "shallow map lost its entry");

        // double (3) and array (5) round trip; an array nests values like a map
        DataStoreChangeValue element;
        element.mType = DataStoreChangeValueType::Double;
        element.mDoubleValue = 2.5;
        DataStoreChangeValue text;
        text.mType = DataStoreChangeValueType::String;
        text.mStringValue = "label";
        DataStoreChangeValue array;
        array.mType = DataStoreChangeValueType::Array;
        array.mArrayValue = {element, text};
        BinaryStream arrayWire;
        DataStoreCodec::writeChangeValue(arrayWire, array);
        ReadOnlyBinaryStream arrayInput(arrayWire.getBuffer());
        require(arrayInput.getLInt() == 5, "array was not written with type 5");
        require(arrayInput.getUnsignedVarInt() == 2, "array count was not a varuint");
        require(arrayInput.getLInt() == 3, "double was not written with type 3");
        require(arrayInput.getLDouble() == 2.5, "double was not written as a little endian double");
        ReadOnlyBinaryStream roundTripInput(arrayWire.getBuffer());
        const DataStoreChangeValue decoded = DataStoreCodec::readChangeValue(roundTripInput);
        require(decoded.mType == DataStoreChangeValueType::Array, "array lost its type");
        require(decoded.mArrayValue.size() == 2, "array lost its entries");
        require(decoded.mArrayValue[0].mType == DataStoreChangeValueType::Double, "array entry lost its type");
        require(decoded.mArrayValue[0].mDoubleValue == 2.5, "array entry lost its value");
        require(decoded.mArrayValue[1].mStringValue == "label", "array string entry lost its value");

        // arrays count toward the nesting bound as maps do
        BinaryStream deepArray;
        for (int i = 0; i < depth; ++i) {
            deepArray.putLInt(5);
            deepArray.putUnsignedVarInt(1);
        }
        deepArray.putLInt(0);
        ReadOnlyBinaryStream deepArrayInput(deepArray.getBuffer());
        bool arrayThrew = false;
        try {
            DataStoreCodec::readChangeValue(deepArrayInput);
        } catch (const BinaryDataException &) {
            arrayThrew = true;
        }
        require(arrayThrew, "deeply nested data store array was not rejected");

        std::cout << "DataStore decode tests passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << std::endl;
        return 1;
    }
}
