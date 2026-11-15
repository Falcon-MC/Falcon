#pragma once

#include "Protocol/Types/ItemStack.h"

#include <string>

class Actor;
class ServerNetworkHandler;

// what the commands that hand out an item do to it beyond its name, count and data value
namespace CommandItems {
    // a firework star takes its color from the data value, a firework rocket its flight
    void applyData(ItemStack &item, int data);

    // a filled map is made for the entity it goes to; false with the answer when the data value names no map
    bool prepareFor(ServerNetworkHandler &owner, Actor &actor, ItemStack &item, std::string &error);
}
