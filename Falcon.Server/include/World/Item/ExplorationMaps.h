#pragma once

#include "World/Level/Map/MapItemSavedData.h"

#include <cstdint>
#include <string>

class Actor;
class ServerNetworkHandler;
class ItemStack;

// sub_1418BA320: filled map aux, marker and target structure of each explorer map
namespace ExplorationMaps {
    struct Entry {
        int32_t mAux;
        MapDecorationType mDecoration;
        // the Falcon structure searched for, empty when Falcon does not place it yet
        const char *mStructure;
        const char *mDestination;
    };

    const Entry *findByAux(int32_t aux);

    const Entry *findByDestination(const std::string &destination);

    // sub_1418BC9A0: a scale 2 map centred on the nearest structure, or on the searcher when there is none
    void create(ServerNetworkHandler &owner, ItemStack &item, const Actor *searcher, const Entry &entry);
}
