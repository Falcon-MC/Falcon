#include "Command/TickingAreaCommand.h"

#include "Level/Level.h"
#include "Level/TickingAreaManager.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
    const char *const BOOLEAN_ENUM = "Boolean";
    const std::vector<std::string> BOOLEAN_VALUES = {"true", "false"};
    const int32_t CHUNK_SIZE = 16;
    const int32_t CHUNK_CENTER_OFFSET = 8;

    std::string triple(int32_t x, int32_t z, const char *separator) {
        return std::to_string(x) + separator + "0" + separator + std::to_string(z);
    }

    bool parseBoolean(const std::string &value, bool &out) {
        if (value == "true") {
            out = true;
            return true;
        }
        if (value == "false") {
            out = false;
            return true;
        }
        return false;
    }

    bool parseInteger(const std::string &value, int32_t &out) {
        char *end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0')
            return false;

        out = (int32_t) parsed;
        return true;
    }

    const char *dimensionLabel(DimensionType dimension) {
        switch (dimension) {
            case DimensionType::Nether:
                return "Nether";
            case DimensionType::TheEnd:
                return "The End";
            default:
                return "Overworld";
        }
    }
}

TickingAreaCommand::TickingAreaCommand(ServerNetworkHandler &handler)
        : Command("tickingarea", "commands.tickingarea.description",
                  "/tickingarea <add|remove|remove_all|list|preload> ..."),
          mHandler(handler) {}

std::vector<CommandOverloadData> TickingAreaCommand::getOverloads() const {
    CommandOverloadData addBounds;
    addBounds.mParameters = {makeEnumParameter("mode", "TickingAreaAdd", {"add"}),
                             makeTypedParameter("from", CommandParamType::BlockPosition),
                             makeTypedParameter("to", CommandParamType::BlockPosition),
                             makeTypedParameter("name", CommandParamType::String, true),
                             makeEnumParameter("preload", BOOLEAN_ENUM, BOOLEAN_VALUES, true)};

    CommandOverloadData addCircle;
    addCircle.mParameters = {makeEnumParameter("mode", "TickingAreaAdd", {"add"}),
                             makeEnumParameter("circle", "TickingAreaCircle", {"circle"}),
                             makeTypedParameter("center", CommandParamType::BlockPosition),
                             makeTypedParameter("radius", CommandParamType::Int),
                             makeTypedParameter("name", CommandParamType::String, true),
                             makeEnumParameter("preload", BOOLEAN_ENUM, BOOLEAN_VALUES, true)};

    CommandOverloadData removePosition;
    removePosition.mParameters = {makeEnumParameter("mode", "TickingAreaRemove", {"remove"}),
                                  makeTypedParameter("position", CommandParamType::BlockPosition)};

    CommandOverloadData removeName;
    removeName.mParameters = {makeEnumParameter("mode", "TickingAreaRemove", {"remove"}),
                              makeTypedParameter("name", CommandParamType::String)};

    CommandOverloadData removeAll;
    removeAll.mParameters = {makeEnumParameter("mode", "TickingAreaRemoveAll", {"remove_all"})};

    CommandOverloadData list;
    list.mParameters = {makeEnumParameter("mode", "TickingAreaList", {"list"}),
                        makeEnumParameter("all-dimensions", "TickingAreaAllDimensions", {"all-dimensions"}, true)};

    CommandOverloadData preloadPosition;
    preloadPosition.mParameters = {makeEnumParameter("mode", "TickingAreaPreload", {"preload"}),
                                   makeTypedParameter("position", CommandParamType::BlockPosition),
                                   makeEnumParameter("preload", BOOLEAN_ENUM, BOOLEAN_VALUES, true)};

    CommandOverloadData preloadName;
    preloadName.mParameters = {makeEnumParameter("mode", "TickingAreaPreload", {"preload"}),
                               makeTypedParameter("name", CommandParamType::String),
                               makeEnumParameter("preload", BOOLEAN_ENUM, BOOLEAN_VALUES, true)};

    return {addBounds, addCircle, removePosition, removeName, removeAll, list, preloadPosition, preloadName};
}

bool TickingAreaCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty() || sender.getLevel() == nullptr) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const Vector3f position = sender.getPosition();
    const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                          (int32_t) std::floor(position.z));
    const std::string &mode = arguments[0];

    if (mode == "add")
        return _add(sender, arguments, origin);
    if (mode == "remove")
        return _remove(sender, arguments, origin);
    if (mode == "remove_all")
        return _removeAll(sender);
    if (mode == "list")
        return _list(sender, arguments);
    if (mode == "preload")
        return _preload(sender, arguments, origin);

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

bool TickingAreaCommand::_add(CommandOrigin &sender, const std::vector<std::string> &arguments,
                              const Vector3i &origin) {
    TickingAreaManager &areas = mHandler.getTickingAreas();
    const bool circle = arguments.size() > 1 && arguments[1] == "circle";
    const size_t optionalIndex = circle ? 6 : 7;

    TickingArea area;
    if (circle) {
        Vector3i center;
        int32_t radius = 0;
        if (arguments.size() < 6 || !parseBlockPosition(arguments, 2, origin, center)
            || !parseInteger(arguments[5], radius) || radius < 0) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        if (radius > TickingAreaManager::MAX_RADIUS) {
            sender.sendTranslation("commands.tickingarea-add.radiusfailure",
                                   {std::to_string(TickingAreaManager::MAX_RADIUS)});
            return false;
        }

        area = TickingArea::circle(center, radius);
    } else {
        Vector3i from;
        Vector3i to;
        if (arguments.size() < 7 || !parseBlockPosition(arguments, 1, origin, from)
            || !parseBlockPosition(arguments, 4, origin, to)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        area = TickingArea::box(from, to);
        const int64_t chunks = (int64_t) ((area.mMaxX - area.mMinX + 1) / CHUNK_SIZE)
                               * ((area.mMaxZ - area.mMinZ + 1) / CHUNK_SIZE);
        if (chunks > TickingAreaManager::MAX_CHUNKS) {
            sender.sendTranslation("commands.tickingarea-add.chunkfailure",
                                   {std::to_string(TickingAreaManager::MAX_CHUNKS)});
            return false;
        }
    }

    if (arguments.size() > optionalIndex + 1 && !parseBoolean(arguments[optionalIndex + 1], area.mPreload)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    area.mName = arguments.size() > optionalIndex ? arguments[optionalIndex] : areas.nextDefaultName();
    area.mDimension = sender.getLevel()->getDimensionType();

    if (areas.getAreas().size() >= TickingAreaManager::MAX_AREAS) {
        sender.sendTranslation("commands.tickingarea-add.failure", {std::to_string(TickingAreaManager::MAX_AREAS)});
        return false;
    }

    if (areas.hasName(area.mName)) {
        sender.sendTranslation("commands.tickingarea-add.conflictingname", {area.mName});
        return false;
    }

    const TickingArea &added = areas.add(mHandler, area);
    if (added.mCircle) {
        const std::string center = triple(added.getCenterChunkX() * CHUNK_SIZE + CHUNK_CENTER_OFFSET,
                                          added.getCenterChunkZ() * CHUNK_SIZE + CHUNK_CENTER_OFFSET, ", ");
        sender.sendTranslation(added.mPreload ? "commands.tickingarea-add-circle.preload.success"
                                              : "commands.tickingarea-add-circle.success",
                               {center, std::to_string(added.getRadius())});
    } else {
        sender.sendTranslation(added.mPreload ? "commands.tickingarea-add-bounds.preload.success"
                                              : "commands.tickingarea-add-bounds.success",
                               {triple(added.mMinX, added.mMinZ, ", "), triple(added.mMaxX, added.mMaxZ, ", ")});
    }

    _sendInUse(sender);
    return true;
}

bool TickingAreaCommand::_remove(CommandOrigin &sender, const std::vector<std::string> &arguments,
                                 const Vector3i &origin) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const DimensionType dimension = sender.getLevel()->getDimensionType();
    Vector3i position;
    const bool byPosition = arguments.size() >= 4 && parseBlockPosition(arguments, 1, origin, position);
    const std::string name = arguments[1];

    const std::vector<TickingArea> removed = mHandler.getTickingAreas().remove(
            mHandler, [&](const TickingArea &area) {
                return area.mDimension == dimension
                       && (byPosition ? area.containsBlock(position) : area.mName == name);
            });

    if (removed.empty()) {
        if (byPosition)
            sender.sendTranslation("commands.tickingarea-remove.failure",
                                   {std::to_string(position.x) + ", " + std::to_string(position.y) + ", "
                                    + std::to_string(position.z)});
        else
            sender.sendTranslation("commands.tickingarea-remove.byname.failure", {name});
        return false;
    }

    sender.sendTranslation("commands.tickingarea-remove.success", {});
    for (const TickingArea &area: removed)
        sender.sendTranslation(_describe(area), {});
    _sendInUse(sender);
    return true;
}

bool TickingAreaCommand::_removeAll(CommandOrigin &sender) {
    const DimensionType dimension = sender.getLevel()->getDimensionType();
    const std::vector<TickingArea> removed = mHandler.getTickingAreas().remove(
            mHandler, [dimension](const TickingArea &area) {
                return area.mDimension == dimension;
            });

    if (removed.empty()) {
        sender.sendTranslation("commands.tickingarea-remove_all.failure", {});
        return false;
    }

    sender.sendTranslation("commands.tickingarea-remove_all.success", {});
    for (const TickingArea &area: removed)
        sender.sendTranslation(_describe(area), {});
    _sendInUse(sender);
    return true;
}

bool TickingAreaCommand::_list(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::vector<TickingArea> &areas = mHandler.getTickingAreas().getAreas();
    const bool allDimensions = arguments.size() > 1 && arguments[1] == "all-dimensions";

    if (!allDimensions) {
        const DimensionType dimension = sender.getLevel()->getDimensionType();
        sender.sendTranslation("§a%commands.tickingarea-list.success.currentDimension", {});
        for (const TickingArea &area: areas) {
            if (area.mDimension == dimension)
                sender.sendTranslation(_describe(area), {});
        }
        _sendInUse(sender);
        return true;
    }

    if (areas.empty()) {
        sender.sendTranslation("commands.tickingarea-list.failure.allDimensions", {});
        return false;
    }

    sender.sendTranslation("§a%commands.tickingarea-list.success.allDimensions", {});
    for (const DimensionType dimension: {DimensionType::Overworld, DimensionType::Nether, DimensionType::TheEnd}) {
        const bool any = std::any_of(areas.begin(), areas.end(), [dimension](const TickingArea &area) {
            return area.mDimension == dimension;
        });
        if (!any)
            continue;

        sender.sendMessage(std::string(dimensionLabel(dimension)) + ": ");
        for (const TickingArea &area: areas) {
            if (area.mDimension == dimension)
                sender.sendTranslation(_describe(area), {});
        }
    }
    _sendInUse(sender);
    return true;
}

bool TickingAreaCommand::_preload(CommandOrigin &sender, const std::vector<std::string> &arguments,
                                  const Vector3i &origin) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const DimensionType dimension = sender.getLevel()->getDimensionType();
    Vector3i position;
    const bool byPosition = arguments.size() >= 4 && parseBlockPosition(arguments, 1, origin, position);
    const size_t valueIndex = byPosition ? 4 : 2;
    const std::string name = arguments[1];

    bool preload = false;
    const bool setting = arguments.size() > valueIndex;
    if (setting && !parseBoolean(arguments[valueIndex], preload)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const auto match = [&](const TickingArea &area) {
        return area.mDimension == dimension && (byPosition ? area.containsBlock(position) : area.mName == name);
    };

    std::vector<TickingArea> matched;
    if (setting) {
        matched = mHandler.getTickingAreas().setPreload(mHandler, match, preload);
    } else {
        for (const TickingArea &area: mHandler.getTickingAreas().getAreas()) {
            if (match(area))
                matched.push_back(area);
        }
    }

    if (matched.empty()) {
        if (byPosition)
            sender.sendTranslation("commands.tickingarea-preload.byposition.failure",
                                   {std::to_string(position.x) + ", " + std::to_string(position.y) + ", "
                                    + std::to_string(position.z)});
        else
            sender.sendTranslation("commands.tickingarea-preload.byname.failure", {name});
        return false;
    }

    if (setting)
        sender.sendTranslation("commands.tickingarea-preload.success", {});

    size_t preloaded = 0;
    for (const TickingArea &area: matched) {
        sender.sendTranslation(_describe(area), {});
        if (area.mPreload)
            ++preloaded;
    }
    sender.sendTranslation("commands.tickingarea-preload.count", {std::to_string(preloaded)});
    return true;
}

void TickingAreaCommand::_sendInUse(CommandOrigin &sender) {
    sender.sendTranslation("commands.tickingarea.inuse",
                           {std::to_string(mHandler.getTickingAreas().getAreas().size()),
                            std::to_string(TickingAreaManager::MAX_AREAS)});
}

std::string TickingAreaCommand::_describe(const TickingArea &area) {
    std::string line = "- " + area.mName;
    if (area.mCircle) {
        line += " (%commands.tickingarea-list.type.circle): "
                + triple(area.getCenterChunkX() * CHUNK_SIZE + CHUNK_CENTER_OFFSET,
                         area.getCenterChunkZ() * CHUNK_SIZE + CHUNK_CENTER_OFFSET, " ")
                + " %commands.tickingarea-list.circle.radius: " + std::to_string(area.getRadius())
                + " %commands.tickingarea-list.chunks";
    } else {
        line += ": " + triple(area.mMinX, area.mMinZ, " ") + " %commands.tickingarea-list.to "
                + triple(area.mMaxX, area.mMaxZ, " ");
    }

    if (area.mPreload)
        line += " %commands.tickingarea-list.preload";
    return line;
}
