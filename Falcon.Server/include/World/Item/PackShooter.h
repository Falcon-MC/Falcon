#pragma once

#include <cstdint>

class ServerNetworkHandler;
class ServerPlayer;
struct CustomItemDefinition;

// minecraft:shooter and minecraft:throwable on a behavior pack item, as the game runs them on use and release
class PackShooter {
public:
    enum class Use { None, Done, Drawing };

    // Drawing: the caller starts the use for duration ticks; Done: shot, thrown or fired its charge
    static Use use(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
                   int32_t &duration);

    // false when neither component draws: the release is not the item's
    static bool release(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
                        int32_t elapsedTicks);
};
