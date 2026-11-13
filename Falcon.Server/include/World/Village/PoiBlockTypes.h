#pragma once

#include <cstdint>
#include <string>
#include <vector>

// a block point of interest type of the Poi experiment, from its poi_block document
struct PoiBlockType {
    uint16_t mId = 0;
    std::string mName;
    // claimable from each instance
    uint8_t mTickets = 0;
    uint8_t mUsableRange = 0;
    std::vector<std::string> mBlocks;
    // its own tags and every tag document listing one of them
    std::vector<std::string> mTags;

    bool hasTag(const std::string &tag) const;
};

class PoiBlockTypes {
public:
    // the id of a type that no document names
    static constexpr uint16_t INVALID_ID = 0xFFFF;

    // by id: PoiTypeRegistry::registerType (sub_14850DE20, 26.60) appends each new type
    static const std::vector<PoiBlockType> &getAll();

    static const PoiBlockType *get(uint16_t id);

    static const PoiBlockType *find(const std::string &name);

    // the type a block of this identifier makes, any state
    static const PoiBlockType *forBlock(const std::string &blockName);
};
