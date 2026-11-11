#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/FillCommand.h"
#include "Server/Commands/SetBlockCommand.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef FALCON_FLUID_TRACE_DIR
#error "FALCON_FLUID_TRACE_DIR names the folder of the BDS traces"
#endif

namespace {
    using Cells = std::map<std::string, std::string>;

    struct Command {
        int mTick;
        std::string mText;
    };

    // a scenario run in BDS 1.26.61: the commands of a script, then the box after every tick it changed
    struct Trace {
        int mTicks = 0;
        int mBox[6] = {0, 0, 0, 0, 0, 0};
        std::string mDimension = "overworld";
        std::vector<Command> mCommands;
        std::vector<std::pair<int, Cells>> mSteps;
    };

    Trace readTrace(const std::string &name) {
        std::ifstream input(std::string(FALCON_FLUID_TRACE_DIR) + "/" + name + ".txt");
        if (!input)
            throw std::runtime_error("missing trace " + name);

        Trace trace;
        Cells current;
        std::string line;
        while (std::getline(input, line)) {
            std::istringstream words(line);
            std::string kind;
            words >> kind;
            if (kind == "ticks") {
                words >> trace.mTicks;
            } else if (kind == "box") {
                for (int &value: trace.mBox)
                    words >> value;
            } else if (kind == "dimension") {
                words >> trace.mDimension;
            } else if (kind == "cmd") {
                Command command;
                words >> command.mTick;
                std::getline(words, command.mText);
                command.mText.erase(0, command.mText.find_first_not_of(' '));
                trace.mCommands.push_back(command);
            } else if (kind == "T") {
                int tick = 0;
                words >> tick;
                std::string cell;
                while (words >> cell) {
                    const size_t equals = cell.find('=');
                    const std::string key = cell.substr(0, equals);
                    const std::string value = cell.substr(equals + 1);
                    if (value == "air")
                        current.erase(key);
                    else
                        current[key] = value;
                }
                trace.mSteps.emplace_back(tick, current);
            }
        }
        return trace;
    }

    std::string withPrefix(const std::string &name) {
        return name.find(':') == std::string::npos ? "minecraft:" + name : name;
    }

    // the game fills every state a command leaves out with its default
    BlockState defaultState(const std::string &name) {
        BlockState state;
        if (!SetBlockCommand::resolveBlock(withPrefix(name), state))
            throw std::runtime_error("unknown block " + name);
        return state;
    }

    void place(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        if (!SetBlockCommand::placeBlock(owner, level, position, state, "replace"))
            throw std::runtime_error("setblock refused at " + std::to_string(position.x) + " "
                                     + std::to_string(position.y) + " " + std::to_string(position.z));
    }

    // ["liquid_depth"=3] in a setblock command
    Tag parseStates(const std::string &text, Tag states) {
        size_t at = 0;
        while ((at = text.find('"', at)) != std::string::npos) {
            const size_t end = text.find('"', at + 1);
            const std::string key = text.substr(at + 1, end - at - 1);
            const size_t valueStart = text.find('=', end) + 1;
            size_t valueEnd = text.find_first_of(",]", valueStart);
            const std::string value = text.substr(valueStart, valueEnd - valueStart);
            if (value == "true" || value == "false")
                states.putByte(key, value == "true" ? 1 : 0);
            else
                states.putInt(key, std::stoi(value));
            at = valueEnd;
        }
        return states;
    }

    // a console in the level of the scenario; a refused command fails the scenario
    class TraceOrigin : public CommandOrigin {
    public:
        explicit TraceOrigin(Level &level) : mLevel(level) {}

        std::string getSenderName() const override {
            return "fluid-parity";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            (void) message;
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            (void) parameters;
            if (key != "commands.fill.success")
                throw std::runtime_error("fill answered " + key);
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }

        Level *getLevel() override {
            return &mLevel;
        }

    private:
        Level &mLevel;
    };

    void runCommand(ServerNetworkHandler &owner, Level &level, const std::string &text) {
        std::istringstream words(text);
        std::string verb;
        words >> verb;
        if (verb == "setblock") {
            int x = 0;
            int y = 0;
            int z = 0;
            std::string name;
            words >> x >> y >> z >> name;
            std::string rest;
            std::getline(words, rest);
            const BlockState base = defaultState(name);
            place(owner, level, Vector3i(x, y, z), BlockState(base.mName, parseStates(rest, base.mStates)));
            return;
        }
        if (verb == "fill") {
            std::vector<std::string> arguments;
            std::string word;
            while (words >> word)
                arguments.push_back(word);
            if (arguments.size() >= 7)
                arguments[6] = withPrefix(arguments[6]);
            TraceOrigin origin(level);
            FillCommand(owner).execute(origin, arguments);
            return;
        }
        if (verb == "waterlog") {
            // Block.setWaterlogged of the script API
            int x = 0;
            int y = 0;
            int z = 0;
            words >> x >> y >> z;
            // the script call gives the water its tick, as a bucket does
            level.setBlockStateAtLayer(x, y, z, 1, defaultState("water"));
            level.scheduleFluidTick(Vector3i(x, y, z), level.getLiquidPhysics().getTickDelay(LiquidMaterial::Water));
            return;
        }
        throw std::runtime_error("unknown command " + text);
    }

    // the cell format of the BDS script: name, liquid_depth, +w when water fills layer 1
    Cells capture(Level &level, const int box[6]) {
        Cells cells;
        for (int y = box[1]; y <= box[4]; ++y) {
            for (int z = box[2]; z <= box[5]; ++z) {
                for (int x = box[0]; x <= box[3]; ++x) {
                    const BlockState &layer0 = *level.peekBlockPtr(x, y, z);
                    const BlockState *layer1 = level.peekBlockPtr(x, y, z, 1);
                    const bool waterlogged = layer1 != nullptr && LiquidProperties::of(*layer1).isLiquid();
                    if (layer0.mName == "minecraft:air" && !waterlogged)
                        continue;

                    std::string value = layer0.mName.substr(layer0.mName.find(':') + 1);
                    if (layer0.mStates.get("liquid_depth") != nullptr)
                        value += ":" + std::to_string(layer0.mStates.getInt("liquid_depth"));
                    if (waterlogged)
                        value += "+w";
                    cells[std::to_string(x - box[0]) + "," + std::to_string(y - box[1]) + ","
                          + std::to_string(z - box[2])] = value;
                }
            }
        }
        return cells;
    }

    std::string describe(const Cells &expected, const Cells &actual) {
        std::string result;
        for (const auto &cell: expected) {
            auto found = actual.find(cell.first);
            if (found == actual.end() || found->second != cell.second)
                result += " " + cell.first + " bds=" + cell.second
                          + " falcon=" + (found == actual.end() ? "air" : found->second);
        }
        for (const auto &cell: actual) {
            if (expected.find(cell.first) == expected.end())
                result += " " + cell.first + " bds=air falcon=" + cell.second;
        }
        return result;
    }

    // a sponge draws from the level random: only how many blocks of each kind there are can match, water of any
    // depth counted together
    Cells histogram(const Cells &cells) {
        std::map<std::string, int> counts;
        for (const auto &cell: cells)
            ++counts[cell.second.substr(0, cell.second.find(':'))];
        Cells result;
        for (const auto &count: counts)
            result[count.first] = std::to_string(count.second);
        return result;
    }

    void replay(const std::string &argument) {
        // ~name@tick: counted, and only until the random shape starts to matter
        const bool counted = argument[0] == '~';
        const size_t at = argument.find('@');
        const std::string name = argument.substr(counted ? 1 : 0,
                                                 at == std::string::npos ? std::string::npos : at - (counted ? 1 : 0));
        Trace trace = readTrace(name);
        if (at != std::string::npos)
            trace.mTicks = std::stoi(argument.substr(at + 1));
        const DimensionType dimension = trace.mDimension == "nether" ? DimensionType::Nether : DimensionType::Overworld;
        ServerNetworkHandler owner("Fluid parity", "", 1);
        Level level("fluid-parity", 4, 1, dimension);
        level.setOwner(&owner);

        // the ticking area of the BDS world: chunks 0 to 2 on both axes
        std::vector<int64_t> columns;
        for (int32_t chunkX = 0; chunkX <= 2; ++chunkX) {
            for (int32_t chunkZ = 0; chunkZ <= 2; ++chunkZ) {
                level.getChunk(chunkX, chunkZ);
                columns.push_back(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
            }
        }
        level.setActiveColumns(columns);

        // the harness clears this much around the box before each scenario
        const int clear = trace.mDimension == "nether" ? 5 : 10;
        for (int x = trace.mBox[0]; x <= trace.mBox[3]; ++x)
            for (int y = trace.mBox[1] - clear; y <= trace.mBox[4] + clear - 1; ++y)
                for (int z = trace.mBox[2]; z <= trace.mBox[5]; ++z)
                    level.setBlockState(x, y, z, BlockState("minecraft:air"));

        size_t step = 0;
        for (int tick = 0; tick < trace.mTicks; ++tick) {
            if (tick > 0)
                level.tick();
            for (const Command &command: trace.mCommands) {
                if (command.mTick == tick)
                    runCommand(owner, level, command.mText);
            }

            while (step + 1 < trace.mSteps.size() && trace.mSteps[step + 1].first <= tick)
                ++step;
            const Cells actual = counted ? histogram(capture(level, trace.mBox)) : capture(level, trace.mBox);
            const Cells expected = counted ? histogram(trace.mSteps[step].second) : trace.mSteps[step].second;
            if (actual != expected)
                throw std::runtime_error(name + " differs at tick " + std::to_string(tick) + ":"
                                         + describe(expected, actual).substr(0, 1500));
        }
    }

    struct BenchResult {
        double mTotal = 0.0;
        double mWorst = 0.0;
        size_t mProcessed = 0;
        size_t mPeakScheduled = 0;
    };

    BenchResult runTicks(Level &level, int ticks) {
        BenchResult result;
        for (int tick = 0; tick < ticks; ++tick) {
            const auto start = std::chrono::steady_clock::now();
            level.tick();
            const double elapsed =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            result.mTotal += elapsed;
            result.mWorst = std::max(result.mWorst, elapsed);
            result.mProcessed += level.getLastFluidProcessedCount();
            result.mPeakScheduled = std::max(result.mPeakScheduled, level.getScheduledFluidCount());
            level.consumeFluidChanges();
        }
        return result;
    }

    void print(const char *name, int ticks, const BenchResult &result) {
        std::cout << std::left << std::setw(10) << name << std::right << std::fixed << std::setprecision(3) << " "
                  << ticks << " ticks  " << result.mTotal << " ms total  " << result.mTotal / ticks << " ms/tick  "
                  << result.mWorst << " ms worst  " << result.mProcessed << " ticks run  " << result.mPeakScheduled
                  << " peak queued\n";
    }

    // not a parity check: the cost of a flood, its drain and a field of waterfalls on 6x6 chunks
    void bench() {
        Level level("fluid-bench", 4, 1);
        std::vector<int64_t> columns;
        for (int32_t chunkX = 0; chunkX < 6; ++chunkX) {
            for (int32_t chunkZ = 0; chunkZ < 6; ++chunkZ) {
                level.getChunk(chunkX, chunkZ);
                columns.push_back(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
            }
        }
        level.setActiveColumns(columns);

        const BlockState air("minecraft:air");
        const BlockState stone("minecraft:stone");
        for (int x = 8; x < 88; ++x)
            for (int z = 8; z < 88; ++z)
                for (int y = 199; y <= 255; ++y)
                    level.setBlockState(x, y, z, y == 199 ? stone : air);

        Tag flowing = Tag::ofCompound();
        flowing.putInt("liquid_depth", 0);
        const BlockState source("minecraft:flowing_water", flowing);
        for (int x = 12; x < 88; x += 8)
            for (int z = 12; z < 88; z += 8)
                level.setBlockState(x, 200, z, source);
        print("flood", 200, runTicks(level, 200));

        for (int x = 12; x < 88; x += 8)
            for (int z = 12; z < 88; z += 8)
                level.setBlockState(x, 200, z, air);
        print("drain", 200, runTicks(level, 200));

        for (int x = 12; x < 88; x += 8)
            for (int z = 12; z < 88; z += 8)
                level.setBlockState(x, 250, z, source);
        print("waterfall", 400, runTicks(level, 400));
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--bench") {
        bench();
        return 0;
    }

    int failures = 0;
    for (int i = 1; i < argc; ++i) {
        try {
            replay(argv[i]);
            std::cout << argv[i] << ": same as BDS on every tick\n";
        } catch (const std::exception &error) {
            std::cout << "FAIL " << error.what() << '\n';
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
