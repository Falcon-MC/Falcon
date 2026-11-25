#include "Core/NBT/NbtIo.h"
#include "Core/NBT/Tag.h"
#include "Core/Json/Json.h"
#include "Core/Utility/BinaryStream.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

    int gFailures = 0;
    int gChecks = 0;

    void check(bool condition, const std::string &name) {
        gChecks++;

        if (!condition) {
            gFailures++;
            std::printf("FAIL: %s\n", name.c_str());
        }
    }

    std::string bytes(std::initializer_list<int> values) {
        std::string result;

        for (int value: values)
            result.push_back((char) (unsigned char) value);

        return result;
    }

    std::string hex(const std::string &data) {
        std::string result;
        char buffer[4];

        for (unsigned char value: data) {
            std::snprintf(buffer, sizeof(buffer), "%02X ", value);
            result += buffer;
        }

        return result;
    }

    const char *variantName(NbtVariant variant) {
        switch (variant) {
            case NbtVariant::BigEndian:
                return "BigEndian";
            case NbtVariant::LittleEndian:
                return "LittleEndian";
            default:
                return "Network";
        }
    }

    std::string encodeTag(const Tag &tag, NbtVariant variant, const std::string &rootName = std::string()) {
        BinaryStream stream;
        NbtIo::writeTag(stream, tag, variant, rootName);
        return stream.getBuffer();
    }

    std::string encodeValue(const Tag &tag, NbtVariant variant) {
        BinaryStream stream;
        NbtIo::writeValue(stream, tag, variant);
        return stream.getBuffer();
    }

    Tag decodeValue(const std::string &data, Tag::Type type, NbtVariant variant) {
        ReadOnlyBinaryStream stream(data);
        return NbtIo::readValue(stream, type, variant);
    }

    bool throwsOnRead(const std::string &data, NbtVariant variant) {
        try {
            ReadOnlyBinaryStream stream(data);
            NbtIo::readTag(stream, variant);
        } catch (const BinaryDataException &) {
            return true;
        }

        return false;
    }

    void checkValueLayout(const std::string &name, const Tag &tag, NbtVariant variant, const std::string &expected) {
        const std::string label = name + " " + variantName(variant);
        const std::string actual = encodeValue(tag, variant);

        check(actual == expected, label + " encodes as " + hex(expected) + "got " + hex(actual));
        check(decodeValue(expected, tag.getType(), variant) == tag, label + " decodes back");
    }

    Tag buildEveryTypeCompound() {
        Tag root = Tag::ofCompound();
        root.putByte("byte", -128);
        root.putShort("short", -32768);
        root.putInt("int", std::numeric_limits<int32_t>::min());
        root.putLong("long", std::numeric_limits<int64_t>::max());
        root.putFloat("float", 3.5f);
        root.putDouble("double", -0.125);
        root.put("byteArray", Tag::ofByteArray({0, 1, -1, 127, -128}));
        root.putString("string", "minecraft:stone");
        root.put("list", Tag::ofList(Tag::Type::Int, {Tag::ofInt(1), Tag::ofInt(-2), Tag::ofInt(300)}));
        root.put("intArray", Tag::ofIntArray({0, -1, std::numeric_limits<int32_t>::max()}));
        root.put("longArray", Tag::ofLongArray({0, -1, std::numeric_limits<int64_t>::min()}));

        Tag nested = Tag::ofCompound();
        nested.putString("name", "minecraft:white_wool");
        nested.putInt("version", 18168865);

        Tag states = Tag::ofCompound();
        states.putString("color", "white");
        nested.put("states", states);
        root.put("compound", nested);

        Tag compoundList = Tag::ofList(Tag::Type::Compound);
        compoundList.addToList(nested);
        compoundList.addToList(Tag::ofCompound());
        root.put("compoundList", compoundList);

        Tag listOfLists = Tag::ofList(Tag::Type::List);
        listOfLists.addToList(Tag::ofList(Tag::Type::String, {Tag::ofString("a"), Tag::ofString("")}));
        listOfLists.addToList(Tag::ofList(Tag::Type::End));
        root.put("listOfLists", listOfLists);

        root.put("emptyList", Tag::ofList(Tag::Type::End));
        root.put("emptyCompound", Tag::ofCompound());
        root.put("emptyByteArray", Tag::ofByteArray({}));

        return root;
    }

    void testRootCompoundLayouts() {
        Tag root = Tag::ofCompound();
        root.putByte("a", 1);

        check(encodeTag(root, NbtVariant::BigEndian) == bytes({0x0A, 0x00, 0x00, 0x01, 0x00, 0x01, 0x61, 0x01, 0x00}),
              "root compound BigEndian layout");
        check(encodeTag(root, NbtVariant::LittleEndian) ==
              bytes({0x0A, 0x00, 0x00, 0x01, 0x01, 0x00, 0x61, 0x01, 0x00}),
              "root compound LittleEndian layout");
        check(encodeTag(root, NbtVariant::Network) == bytes({0x0A, 0x00, 0x01, 0x01, 0x61, 0x01, 0x00}),
              "root compound Network layout");

        check(encodeTag(root, NbtVariant::Network, "ab") == bytes({0x0A, 0x02, 0x61, 0x62, 0x01, 0x01, 0x61, 0x01, 0x00}),
              "named root Network layout");

        std::string rootName;
        ReadOnlyBinaryStream stream(bytes({0x0A, 0x00, 0x02, 0x61, 0x62, 0x01, 0x00, 0x01, 0x61, 0x01, 0x00}));
        const Tag decoded = NbtIo::readTag(stream, NbtVariant::BigEndian, &rootName);

        check(rootName == "ab", "BigEndian root name is read");
        check(decoded == root, "BigEndian named root decodes");
        check(stream.feof(), "BigEndian root consumes the whole buffer");
    }

    void testScalarLayouts() {
        checkValueLayout("byte", Tag::ofByte(-1), NbtVariant::BigEndian, bytes({0xFF}));
        checkValueLayout("byte", Tag::ofByte(-1), NbtVariant::LittleEndian, bytes({0xFF}));
        checkValueLayout("byte", Tag::ofByte(-1), NbtVariant::Network, bytes({0xFF}));

        checkValueLayout("short", Tag::ofShort(0x1234), NbtVariant::BigEndian, bytes({0x12, 0x34}));
        checkValueLayout("short", Tag::ofShort(0x1234), NbtVariant::LittleEndian, bytes({0x34, 0x12}));
        checkValueLayout("short", Tag::ofShort(0x1234), NbtVariant::Network, bytes({0x34, 0x12}));

        checkValueLayout("int", Tag::ofInt(300), NbtVariant::BigEndian, bytes({0x00, 0x00, 0x01, 0x2C}));
        checkValueLayout("int", Tag::ofInt(300), NbtVariant::LittleEndian, bytes({0x2C, 0x01, 0x00, 0x00}));
        checkValueLayout("int", Tag::ofInt(300), NbtVariant::Network, bytes({0xD8, 0x04}));
        checkValueLayout("int -1", Tag::ofInt(-1), NbtVariant::Network, bytes({0x01}));
        checkValueLayout("int min", Tag::ofInt(std::numeric_limits<int32_t>::min()), NbtVariant::Network,
                         bytes({0xFF, 0xFF, 0xFF, 0xFF, 0x0F}));

        checkValueLayout("long", Tag::ofLong(1), NbtVariant::BigEndian,
                         bytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));
        checkValueLayout("long", Tag::ofLong(1), NbtVariant::LittleEndian,
                         bytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
        checkValueLayout("long", Tag::ofLong(1), NbtVariant::Network, bytes({0x02}));
        checkValueLayout("long min", Tag::ofLong(std::numeric_limits<int64_t>::min()), NbtVariant::Network,
                         bytes({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}));

        checkValueLayout("float", Tag::ofFloat(1.0f), NbtVariant::BigEndian, bytes({0x3F, 0x80, 0x00, 0x00}));
        checkValueLayout("float", Tag::ofFloat(1.0f), NbtVariant::LittleEndian, bytes({0x00, 0x00, 0x80, 0x3F}));
        checkValueLayout("float", Tag::ofFloat(1.0f), NbtVariant::Network, bytes({0x00, 0x00, 0x80, 0x3F}));

        checkValueLayout("double", Tag::ofDouble(1.0), NbtVariant::BigEndian,
                         bytes({0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
        checkValueLayout("double", Tag::ofDouble(1.0), NbtVariant::LittleEndian,
                         bytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F}));
        checkValueLayout("double", Tag::ofDouble(1.0), NbtVariant::Network,
                         bytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F}));

        checkValueLayout("string", Tag::ofString("hi"), NbtVariant::BigEndian, bytes({0x00, 0x02, 0x68, 0x69}));
        checkValueLayout("string", Tag::ofString("hi"), NbtVariant::LittleEndian, bytes({0x02, 0x00, 0x68, 0x69}));
        checkValueLayout("string", Tag::ofString("hi"), NbtVariant::Network, bytes({0x02, 0x68, 0x69}));
        checkValueLayout("empty string", Tag::ofString(""), NbtVariant::Network, bytes({0x00}));
    }

    void testArrayAndListLayouts() {
        const Tag byteArray = Tag::ofByteArray({1, -1});
        checkValueLayout("byte array", byteArray, NbtVariant::BigEndian, bytes({0x00, 0x00, 0x00, 0x02, 0x01, 0xFF}));
        checkValueLayout("byte array", byteArray, NbtVariant::LittleEndian,
                         bytes({0x02, 0x00, 0x00, 0x00, 0x01, 0xFF}));
        checkValueLayout("byte array", byteArray, NbtVariant::Network, bytes({0x04, 0x01, 0xFF}));

        const Tag intArray = Tag::ofIntArray({1, -1});
        checkValueLayout("int array", intArray, NbtVariant::BigEndian,
                         bytes({0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF}));
        checkValueLayout("int array", intArray, NbtVariant::LittleEndian,
                         bytes({0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}));
        checkValueLayout("int array", intArray, NbtVariant::Network, bytes({0x04, 0x02, 0x01}));

        const Tag longArray = Tag::ofLongArray({1});
        checkValueLayout("long array", longArray, NbtVariant::BigEndian,
                         bytes({0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));
        checkValueLayout("long array", longArray, NbtVariant::LittleEndian,
                         bytes({0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
        checkValueLayout("long array", longArray, NbtVariant::Network, bytes({0x02, 0x02}));

        const Tag list = Tag::ofList(Tag::Type::Short, {Tag::ofShort(1), Tag::ofShort(2)});
        checkValueLayout("short list", list, NbtVariant::BigEndian,
                         bytes({0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02}));
        checkValueLayout("short list", list, NbtVariant::LittleEndian,
                         bytes({0x02, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00}));
        checkValueLayout("short list", list, NbtVariant::Network, bytes({0x02, 0x04, 0x01, 0x00, 0x02, 0x00}));

        const Tag emptyList = Tag::ofList(Tag::Type::End);
        checkValueLayout("empty list", emptyList, NbtVariant::BigEndian, bytes({0x00, 0x00, 0x00, 0x00, 0x00}));
        checkValueLayout("empty list", emptyList, NbtVariant::LittleEndian, bytes({0x00, 0x00, 0x00, 0x00, 0x00}));
        checkValueLayout("empty list", emptyList, NbtVariant::Network, bytes({0x00, 0x00}));

        const Tag decodedEmpty = decodeValue(bytes({0x00, 0x00}), Tag::Type::List, NbtVariant::Network);
        check(decodedEmpty.getListType() == Tag::Type::End, "empty list keeps the END element type");
        check(decodedEmpty.getList().empty(), "empty list has no elements");

        const Tag typedEmpty = decodeValue(bytes({0x08, 0x00}), Tag::Type::List, NbtVariant::Network);
        check(typedEmpty.getListType() == Tag::Type::String, "empty typed list keeps its element type");
    }

    void testEveryTypeRoundTrip() {
        const Tag root = buildEveryTypeCompound();

        for (NbtVariant variant: {NbtVariant::BigEndian, NbtVariant::LittleEndian, NbtVariant::Network}) {
            const std::string label = std::string("every type round trip ") + variantName(variant);
            const std::string encoded = encodeTag(root, variant, "root");

            std::string rootName;
            ReadOnlyBinaryStream stream(encoded);
            const Tag decoded = NbtIo::readTag(stream, variant, &rootName);

            check(decoded == root, label);
            check(rootName == "root", label + " root name");
            check(stream.feof(), label + " consumes the whole buffer");
            check(decoded.getKeys() == root.getKeys(), label + " keeps compound key order");
            check(encodeTag(decoded, variant, "root") == encoded, label + " re-encodes identically");
            check(decoded.get("compound")->get("states")->getString("color") == "white", label + " nested value");
        }
    }

    void testNaNRoundTrip() {
        const Tag nan = Tag::ofFloat(std::numeric_limits<float>::quiet_NaN());

        for (NbtVariant variant: {NbtVariant::BigEndian, NbtVariant::LittleEndian, NbtVariant::Network}) {
            const Tag decoded = decodeValue(encodeValue(nan, variant), Tag::Type::Float, variant);
            check(std::isnan(decoded.asFloat()), std::string("NaN float survives ") + variantName(variant));
            check(decoded == nan, std::string("NaN float compares equal by bits ") + variantName(variant));
        }
    }

    Tag nestedCompounds(int depth) {
        Tag tag = Tag::ofCompound();

        for (int i = 0; i < depth; i++) {
            Tag parent = Tag::ofCompound();
            parent.put("c", tag);
            tag = parent;
        }

        return tag;
    }

    void testMaxDepth() {
        for (NbtVariant variant: {NbtVariant::BigEndian, NbtVariant::LittleEndian, NbtVariant::Network}) {
            const std::string label = std::string("max depth ") + variantName(variant);

            const Tag allowed = nestedCompounds(NbtIo::MAX_DEPTH);
            const std::string encoded = encodeTag(allowed, variant);
            ReadOnlyBinaryStream stream(encoded);
            check(NbtIo::readTag(stream, variant) == allowed, label + " accepts 16 nested compounds");

            bool writeThrew = false;
            try {
                encodeTag(nestedCompounds(NbtIo::MAX_DEPTH + 1), variant);
            } catch (const NbtException &) {
                writeThrew = true;
            }
            check(writeThrew, label + " refuses to write 17 nested compounds");

            std::string tooDeep = bytes({0x0A});
            tooDeep += variant == NbtVariant::Network ? bytes({0x00}) : bytes({0x00, 0x00});

            for (int i = 0; i <= NbtIo::MAX_DEPTH; i++) {
                tooDeep += bytes({0x0A});
                tooDeep += variant == NbtVariant::Network ? bytes({0x01, 0x63})
                                                          : variant == NbtVariant::BigEndian ? bytes({0x00, 0x01, 0x63})
                                                                                             : bytes({0x01, 0x00, 0x63});
            }

            for (int i = 0; i <= NbtIo::MAX_DEPTH + 1; i++)
                tooDeep += bytes({0x00});

            check(throwsOnRead(tooDeep, variant), label + " refuses to read 17 nested compounds");
        }
    }

    void testMalformedInput() {
        check(throwsOnRead(bytes({0x0D, 0x00, 0x00}), NbtVariant::BigEndian), "unknown root tag id is rejected");
        check(throwsOnRead(bytes({0x0A, 0x00, 0x00, 0x01}), NbtVariant::BigEndian), "truncated compound is rejected");
        check(throwsOnRead(bytes({0x09, 0x00, 0x0E, 0x00}), NbtVariant::Network), "unknown list type is rejected");
        check(throwsOnRead(bytes({0x07, 0x00, 0x01}), NbtVariant::Network), "negative array length is rejected");
        check(throwsOnRead(bytes({0x09, 0x00, 0x00, 0x01}), NbtVariant::Network), "non-empty list of END is rejected");

        ReadOnlyBinaryStream stream(bytes({0x08, 0x00, 0xFF, 0xFF, 0x03}));
        EncodingSettings settings;
        settings.mMaxStringLength = 16;
        stream.setEncodingSettings(settings);

        bool threw = false;
        try {
            NbtIo::readTag(stream, NbtVariant::Network);
        } catch (const BinaryDataException &) {
            threw = true;
        }
        check(threw, "string longer than the encoding limit is rejected");
    }

    void testJsonDepthLimit() {
        // a deeply nested document must be rejected instead of overflowing the stack while parsing
        const std::string deep(json::Parser::MAX_DEPTH + 50, '[');
        check(json::parse(deep) == nullptr, "over-deep JSON array nesting is rejected");

        const std::string shallowOpen(8, '[');
        const std::string shallow = shallowOpen + std::string(8, ']');
        check(json::parse(shallow) != nullptr, "moderate JSON nesting still parses");

        std::string nestedObjects;
        for (size_t i = 0; i < json::Parser::MAX_DEPTH + 50; i++)
            nestedObjects += "{\"a\":";
        nestedObjects += "1";
        for (size_t i = 0; i < json::Parser::MAX_DEPTH + 50; i++)
            nestedObjects += "}";
        check(json::parse(nestedObjects) == nullptr, "over-deep JSON object nesting is rejected");
    }

    void testTagApi() {
        Tag compound = Tag::ofCompound();
        compound.putInt("b", 1);
        compound.putInt("a", 2);
        compound.putInt("b", 3);

        check(compound.size() == 2, "put replaces an existing key");
        check(compound.getKeys()[0] == "b" && compound.getKeys()[1] == "a", "compound keeps insertion order");
        check(compound.getInt("b") == 3, "replaced value is visible");
        check(compound.getString("b", "fallback") == "fallback", "typed getter falls back on type mismatch");
        check(compound.contains("a", Tag::Type::Int), "contains with type");
        check(!compound.contains("a", Tag::Type::Byte), "contains with wrong type");
        check(compound.remove("a") && !compound.contains("a"), "remove deletes the key");
        check(!compound.remove("missing"), "remove reports a missing key");

        bool threw = false;
        try {
            compound.put("end", Tag());
        } catch (const NbtException &) {
            threw = true;
        }
        check(threw, "END tag cannot be stored in a compound");

        threw = false;
        try {
            Tag::ofList(Tag::Type::Int, {Tag::ofByte(1)});
        } catch (const NbtException &) {
            threw = true;
        }
        check(threw, "list element type mismatch is rejected");

        Tag list = Tag::ofList(Tag::Type::End);
        list.addToList(Tag::ofString("x"));
        check(list.getListType() == Tag::Type::String, "first element fixes the list type of an empty list");

        check(Tag::ofByte(1).toString() == "1b", "byte toString");
        check(Tag::ofList(Tag::Type::Int, {Tag::ofInt(1), Tag::ofInt(2)}).toString() == "[1,2]", "list toString");
    }

}

int main() {
    testRootCompoundLayouts();
    testScalarLayouts();
    testArrayAndListLayouts();
    testEveryTypeRoundTrip();
    testNaNRoundTrip();
    testMaxDepth();
    testMalformedInput();
    testJsonDepthLimit();
    testTagApi();

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
