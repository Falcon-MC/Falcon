#include "Server/Commands/SpreadPlayersCommand.h"

#include "Falcon/Plugin/PluginEvents.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>

namespace {
    const float WORLD_LIMIT = 30000000.0f;

    // blocks that hurt or swallow whoever lands in them, as the game lists them
    const char *const HAZARDS[] = {"minecraft:magma",       "minecraft:sweet_berry_bush", "minecraft:wither_rose",
                                   "minecraft:water",       "minecraft:flowing_water",    "minecraft:lava",
                                   "minecraft:flowing_lava"};

    std::string twoDecimals(float value) {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.2f", (double) value);
        return buffer;
    }

    bool parseNumber(const std::string &value, float &out) {
        if (value.empty())
            return false;
        char *end = nullptr;
        errno = 0;
        out = std::strtof(value.c_str(), &end);
        return end != value.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(out);
    }

    bool isHazard(const BlockState &state) {
        for (const char *hazard: HAZARDS) {
            if (state.mName == hazard)
                return true;
        }
        BlockMaterialType type;
        return BlockMaterial::find(state, type)
               && (type == BlockMaterialType::PowderSnow || type == BlockMaterialType::Portal);
    }

    float nextFloat() {
        static std::mt19937 generator{std::random_device{}()};
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(generator);
    }

    using Point = SpreadPlayersCommand::Point;

    float distance(const Point &a, const Point &b) {
        const float dx = a.mX - b.mX;
        const float dz = a.mZ - b.mZ;
        return std::sqrt(dx * dx + dz * dz);
    }
}

SpreadPlayersCommand::SpreadPlayersCommand(ServerNetworkHandler &handler)
        : Command("spreadplayers", "commands.spreadplayers.description",
                  "/spreadplayers <x> <z> <spreadDistance> <maxRange> <victim> [maxHeight]"),
          mHandler(handler), mRandom(nextFloat) {}

std::vector<CommandOverloadData> SpreadPlayersCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeTypedParameter("x", CommandParamType::Value));
    overload.mParameters.push_back(makeTypedParameter("z", CommandParamType::Value));
    overload.mParameters.push_back(makeTypedParameter("spreadDistance", CommandParamType::Float));
    overload.mParameters.push_back(makeTypedParameter("maxRange", CommandParamType::Float));
    overload.mParameters.push_back(makeTypedParameter("victim", CommandParamType::Target));
    overload.mParameters.push_back(makeTypedParameter("maxHeight", CommandParamType::Value, true));
    return {overload};
}

bool SpreadPlayersCommand::isSafe(Level &level, int32_t x, int32_t z, int32_t top) {
    level.getChunk(x >> 4, z >> 4);
    const int32_t surface = level.getHeightAt(x, z);
    if ((BlockMaterial::flagsOf(level.getBlockState(x, surface - 1, z)) & BlockMaterial::FLAG_LIQUID) != 0)
        return false;
    if (surface < level.getMinY())
        return false;

    const int32_t ground = level.getAboveTopSolidBlock(x, top, z);
    if (ground <= level.getMinY())
        return false;
    // from the block underfoot to the one over the head of whoever lands
    for (int32_t y = ground - 1; y <= ground + 2; ++y) {
        if (isHazard(level.getBlockState(x, y, z)))
            return false;
    }
    return !BlockSupport::isSolid(level.getBlockState(x, ground + 1, z));
}

float SpreadPlayersCommand::landingY(Level &level, int32_t x, int32_t z, int32_t top) {
    level.getChunk(x >> 4, z >> 4);
    return (float) level.getAboveTopSolidBlock(x, top, z) + 1.0f;
}

bool SpreadPlayersCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() != 5 && arguments.size() != 6) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const Vector3f originPosition = sender.getPosition();
    const Vector3i origin((int32_t) std::floor(originPosition.x), (int32_t) std::floor(originPosition.y),
                          (int32_t) std::floor(originPosition.z));
    float centreX = 0.0f;
    float centreZ = 0.0f;
    float spreadDistance = 0.0f;
    float maxRange = 0.0f;
    if (!parseCoordinate(arguments[0], (float) origin.x, centreX)
        || !parseCoordinate(arguments[1], (float) origin.z, centreZ) || !parseNumber(arguments[2], spreadDistance)
        || !parseNumber(arguments[3], maxRange)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (spreadDistance < 0.0f) {
        sender.sendTranslation("commands.generic.num.tooSmall", {twoDecimals(spreadDistance), twoDecimals(0.0f)});
        return false;
    }
    if (spreadDistance + 1.0f > maxRange) {
        sender.sendTranslation("commands.generic.num.tooSmall", {twoDecimals(maxRange), twoDecimals(spreadDistance + 1.0f)});
        return false;
    }

    const std::vector<ServerPlayer *> players = mHandler.resolveTargets(sender, arguments[4]);
    const std::vector<ServerActor *> actors = mHandler.resolveActorTargets(sender, arguments[4]);
    const int32_t count = (int32_t) (players.size() + actors.size());
    if (count == 0) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    Level *found = sender.getLevel();
    if (found == nullptr) {
        ServerPlayer *self = sender.asPlayer();
        found = self != nullptr ? &mHandler.getLevelFor(*self) : &mHandler.getLevel();
    }
    Level &level = *found;

    // with no maximum height the search starts under the world's ceiling
    int32_t top = level.getMaxY();
    if (arguments.size() == 6) {
        float maxHeight = 0.0f;
        if (!parseCoordinate(arguments[5], (float) origin.y, maxHeight)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        top = (int32_t) std::floor(maxHeight);
    }

    const float minX = centreX - maxRange;
    const float minZ = centreZ - maxRange;
    const float maxX = centreX + maxRange;
    const float maxZ = centreZ + maxRange;
    const float width = maxX - minX;
    const float depth = maxZ - minZ;
    const auto randomPoint = [&]() {
        const float x = mRandom();
        const float z = mRandom();
        return Point{x * width + minX, z * depth + minZ};
    };

    std::vector<Point> points;
    for (int32_t index = 0; index < count; ++index)
        points.push_back(randomPoint());

    // each round pushes every point a step away from those too close, back into the square; once none moves,
    // the ones over unsafe ground are thrown again
    float closest = std::numeric_limits<float>::max();
    int32_t round = 0;
    bool settled = false;
    for (; round < MAX_ROUNDS; ++round) {
        // the failure reports the closest two of the last round
        closest = std::numeric_limits<float>::max();
        bool moved = false;
        for (size_t index = 0; index < points.size(); ++index) {
            float awayX = 0.0f;
            float awayZ = 0.0f;
            int32_t neighbours = 0;
            for (size_t other = 0; other < points.size(); ++other) {
                if (other == index)
                    continue;
                const float apart = distance(points[other], points[index]);
                closest = std::min(closest, apart);
                if (apart < spreadDistance) {
                    ++neighbours;
                    awayX += points[other].mX - points[index].mX;
                    awayZ += points[other].mZ - points[index].mZ;
                }
            }

            Point point = points[index];
            if (neighbours > 0) {
                awayX /= (float) neighbours;
                awayZ /= (float) neighbours;
                const float length = std::sqrt(awayX * awayX + awayZ * awayZ);
                if (length <= 0.0f) {
                    point = randomPoint();
                } else {
                    point.mX -= awayX / length;
                    point.mZ -= awayZ / length;
                }
                moved = true;
            }
            const Point inside{std::clamp(point.mX, minX, maxX), std::clamp(point.mZ, minZ, maxZ)};
            if (inside.mX != point.mX || inside.mZ != point.mZ)
                moved = true;
            points[index] = inside;
        }
        if (moved)
            continue;

        bool thrown = false;
        for (Point &point: points) {
            if (!isSafe(level, (int32_t) std::floor(point.mX), (int32_t) std::floor(point.mZ), top)) {
                point = randomPoint();
                thrown = true;
            }
        }
        if (!thrown) {
            settled = true;
            break;
        }
    }

    if (!settled) {
        if (closest >= std::numeric_limits<float>::max()) {
            sender.sendTranslation("commands.spreadplayers.failure.players.unsafe",
                                   {std::to_string(count), twoDecimals(centreX), twoDecimals(centreZ),
                                    std::to_string(round)});
        } else {
            sender.sendTranslation("commands.spreadplayers.failure.players",
                                   {std::to_string(count), twoDecimals(centreX), twoDecimals(centreZ),
                                    twoDecimals(closest)});
        }
        return false;
    }

    const auto landing = [&](const Point &point) {
        const int32_t x = (int32_t) std::floor(point.mX);
        const int32_t z = (int32_t) std::floor(point.mZ);
        return Vector3f(std::clamp((float) x + 0.5f, -WORLD_LIMIT, WORLD_LIMIT), landingY(level, x, z, top),
                        std::clamp((float) z + 0.5f, -WORLD_LIMIT, WORLD_LIMIT));
    };
    size_t next = 0;
    for (ServerPlayer *player: players) {
        Vector3f destination = landing(points[next++]);
        if (PluginEvents::teleport(*player, destination, "command"))
            player->teleport(mHandler, destination);
    }
    for (ServerActor *actor: actors) {
        actor->teleport(landing(points[next++]));
        mHandler.broadcastActorMove(*actor, true);
    }

    sender.sendTranslation("commands.spreadplayers.success.players",
                           {std::to_string(count), twoDecimals(centreX), twoDecimals(centreZ)});
    return true;
}
