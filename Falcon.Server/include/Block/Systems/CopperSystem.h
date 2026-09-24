#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

class Level;
class ServerNetworkHandler;

class CopperSystem {
public:
    static const int32_t WAX_ON_EVENT = 2030;
    static const int32_t WAX_OFF_EVENT = 2031;
    static const int32_t SCRAPE_EVENT = 2032;

    static std::string waxedOf(const std::string &identifier);

    static std::string withoutWaxOf(const std::string &identifier);

    static std::string scrapedOf(const std::string &identifier);

    static std::string oxidizedOf(const std::string &identifier);

    static BlockState transform(const BlockState &source, const std::string &identifier);

    static BlockState replaceWithPair(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &source, const std::string &identifier);

    static void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state);

private:
    static bool isWeathering(const std::string &identifier);

    static int32_t oxidationLevel(const std::string &identifier);
};
