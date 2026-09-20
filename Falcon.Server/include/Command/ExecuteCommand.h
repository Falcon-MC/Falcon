#pragma once

#include "Command/Command.h"

class Level;
class ServerNetworkHandler;
class ServerPlayer;

class ExecuteCommand : public Command {
public:
    explicit ExecuteCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    struct Context {
        ServerPlayer *mExecutor;
        Vector3f mPosition;
        Vector3f mRotation;
        Level *mLevel;
    };

    static constexpr int64_t MAX_COMPARED_BLOCKS = 16 * 16 * 256 * 8;

    bool run(CommandOrigin &sender, Context context, const std::vector<std::string> &arguments, size_t index,
             int32_t &successes);

    bool runChain(CommandOrigin &sender, const Context &context, const std::vector<std::string> &arguments,
                  size_t index, int32_t &successes);

    bool testBlock(CommandOrigin &sender, const Context &context, const std::vector<std::string> &arguments,
                   size_t index, size_t &next, bool &matched);

    bool testBlocks(CommandOrigin &sender, const Context &context, const std::vector<std::string> &arguments,
                    size_t index, size_t &next, bool &matched, int32_t &count);

    Level &resolveLevel(const Context &context) const;

    ServerNetworkHandler &mHandler;
};
