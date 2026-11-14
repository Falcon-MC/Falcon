#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginCommand.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Codec/ProtocolCodecRegistry.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    constexpr uint32_t MAX_RECIPE_SIDE = 3;

    std::string lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return value;
    }

    bool hasText(const char *value) {
        return value != nullptr && *value != '\0';
    }

    // the name the plugin registered, or the prefixed one the server gave it when the plain name was taken
    PluginCommand *commandOf(LoadedPlugin &owned, const std::string &name) {
        const std::string candidates[2] = {name, lower(owned.mDescription.mName) + ":" + name};
        for (const std::string &candidate: candidates) {
            if (std::find(owned.mCommands.begin(), owned.mCommands.end(), candidate) == owned.mCommands.end())
                continue;
            auto *command = dynamic_cast<PluginCommand *>(owner().getCommands().getCommand(candidate));
            if (command != nullptr && &command->getPlugin() == &owned)
                return command;
        }
        return nullptr;
    }

    bool toParameter(const std::string &command, const FalconCommandParameter &source, CommandParamData &out) {
        if (!hasText(source.name))
            return false;

        const bool optional = source.optional != 0;
        switch (source.type) {
            case FALCON_COMMAND_PARAM_INT:
                out = Command::makeTypedParameter(source.name, CommandParamType::Int, optional);
                return true;
            case FALCON_COMMAND_PARAM_FLOAT:
                out = Command::makeTypedParameter(source.name, CommandParamType::Float, optional);
                return true;
            case FALCON_COMMAND_PARAM_STRING:
                out = Command::makeTypedParameter(source.name, CommandParamType::String, optional);
                return true;
            case FALCON_COMMAND_PARAM_BOOL:
                out = Command::makeEnumParameter(source.name, "Boolean", {"true", "false"}, optional);
                return true;
            case FALCON_COMMAND_PARAM_TARGET:
                out = Command::makePlayerParameter(source.name);
                out.mOptional = optional;
                return true;
            case FALCON_COMMAND_PARAM_POSITION:
                out = Command::makeTypedParameter(source.name, CommandParamType::Position, optional);
                return true;
            case FALCON_COMMAND_PARAM_BLOCK_POSITION:
                out = Command::makeTypedParameter(source.name, CommandParamType::BlockPosition, optional);
                return true;
            case FALCON_COMMAND_PARAM_MESSAGE:
                out = Command::makeTypedParameter(source.name, CommandParamType::Message, optional);
                return true;
            case FALCON_COMMAND_PARAM_JSON:
                out = Command::makeTypedParameter(source.name, CommandParamType::Json, optional);
                return true;
            case FALCON_COMMAND_PARAM_ENUM: {
                if (source.valueCount == 0 || source.values == nullptr)
                    return false;
                std::vector<std::string> values;
                for (uint32_t index = 0; index < source.valueCount; ++index) {
                    if (!hasText(source.values[index]))
                        return false;
                    values.emplace_back(source.values[index]);
                }
                // enums are shared by name in the command packet: one per command parameter
                out = Command::makeEnumParameter(source.name, command + "_" + source.name, values, optional);
                return true;
            }
            default:
                return false;
        }
    }

    void resendCommands() {
        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned())
                owner()._sendAvailableCommands(entry.second);
        }
    }

    int commandAddOverload(FalconPlugin *source, const char *command, const FalconCommandParameter *parameters,
                           uint32_t count) {
        if (!hasText(command) || (count > 0 && parameters == nullptr))
            return 0;

        PluginCommand *target = commandOf(*plugin(source), lower(command));
        if (target == nullptr)
            return 0;

        CommandOverloadData overload;
        for (uint32_t index = 0; index < count; ++index) {
            CommandParamData parameter;
            if (!toParameter(target->getName(), parameters[index], parameter))
                return 0;
            overload.mParameters.push_back(std::move(parameter));
        }

        target->addOverload(std::move(overload));
        resendCommands();
        return 1;
    }

    int serverExecuteCommand(const char *commandLine) {
        if (!hasText(commandLine))
            return 0;
        ServerCommandOrigin sender(&owner());
        return owner().getCommands().dispatch(sender, commandLine) ? 1 : 0;
    }

    int playerExecuteCommand(FalconPlayer *target, const char *commandLine) {
        if (!hasText(commandLine))
            return 0;
        PlayerCommandOrigin sender(owner(), *player(target), CommandOriginData());
        return owner().getCommands().dispatch(sender, commandLine) ? 1 : 0;
    }

    bool knownItem(const char *identifier) {
        return hasText(identifier) && owner().getItemDefinitions().getDefinition(identifier) != nullptr;
    }

    bool isVanillaRecipe(const std::string &identifier) {
        const CraftingRecipeData *recipes = CraftingRecipeTable::getRecipes();
        for (size_t index = 0; index < CraftingRecipeTable::getRecipeCount(); ++index) {
            if (recipes[index].mRecipeId != nullptr && identifier == recipes[index].mRecipeId)
                return true;
        }
        return false;
    }

    // the recipe book is built once and shared by protocol: drop it and send the new one to everyone online
    void refreshRecipes() {
        owner().getCachedCraftingData() = CraftingDataPacket();
        owner().getRecipeOutputsMutable().clear();
        owner().getRecipeSourceIndicesMutable().clear();
        owner().getStonecutterRecipeIndicesMutable().clear();
        owner().getVanillaRecipeIdsMutable().clear();
        for (const int protocol: ProtocolCodecRegistry::instance().getProtocolVersions())
            owner().getSharedLoginPacket(ServerNetworkHandler::SharedLoginPacket::CraftingData, protocol).reset();

        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned())
                LoginHandler::sendCraftingData(owner(), entry.second);
        }
    }

    int registerRecipe(FalconPlugin *, const FalconRecipeDescriptor *descriptor) {
        if (descriptor == nullptr || !hasText(descriptor->identifier) || !knownItem(descriptor->result)
            || descriptor->resultCount == 0 || descriptor->resultCount > 64 || descriptor->ingredientCount == 0
            || descriptor->ingredients == nullptr)
            return 0;

        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        const std::string identifier = descriptor->identifier;
        for (const CustomRecipe &existing: registry.getRecipes()) {
            if (existing.mIdentifier == identifier)
                return 0;
        }

        CustomRecipe recipe;
        recipe.mIdentifier = identifier;
        recipe.mShaped = descriptor->shaped != 0;
        if (recipe.mShaped) {
            const uint32_t width = descriptor->width;
            const uint32_t height = descriptor->height;
            if (width == 0 || height == 0 || width > MAX_RECIPE_SIDE || height > MAX_RECIPE_SIDE
                || descriptor->ingredientCount != width * height)
                return 0;
            recipe.mWidth = (int32_t) descriptor->width;
            recipe.mHeight = (int32_t) descriptor->height;
        } else if (descriptor->ingredientCount > MAX_RECIPE_SIDE * MAX_RECIPE_SIDE) {
            return 0;
        }

        bool anyItem = false;
        for (uint32_t index = 0; index < descriptor->ingredientCount; ++index) {
            const char *item = descriptor->ingredients[index];
            CustomRecipeIngredient ingredient;
            if (!hasText(item)) {
                if (!recipe.mShaped)
                    return 0;
                ingredient.mEmpty = true;
            } else if (!knownItem(item)) {
                return 0;
            } else {
                ingredient.mItemId = item;
                anyItem = true;
            }
            recipe.mInputs.push_back(std::move(ingredient));
        }
        if (!anyItem)
            return 0;

        recipe.mResultItem = descriptor->result;
        recipe.mResultCount = (int32_t) descriptor->resultCount;
        registry.getRecipesMutable().push_back(std::move(recipe));
        refreshRecipes();
        return 1;
    }

    int removeRecipe(const char *identifier) {
        if (!hasText(identifier))
            return 0;

        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        std::vector<CustomRecipe> &recipes = registry.getRecipesMutable();
        const auto found = std::find_if(recipes.begin(), recipes.end(), [identifier](const CustomRecipe &recipe) {
            return recipe.mIdentifier == identifier;
        });
        if (found != recipes.end()) {
            recipes.erase(found);
        } else if (!isVanillaRecipe(identifier) || !registry.getDisabledRecipes().insert(identifier).second) {
            return 0;
        }

        refreshRecipes();
        return 1;
    }
}

void PluginServerApi::fillCommands(FalconServerApi &api) {
    api.commandAddOverload = &commandAddOverload;
    api.serverExecuteCommand = &serverExecuteCommand;
    api.playerExecuteCommand = &playerExecuteCommand;
    api.registerRecipe = &registerRecipe;
    api.removeRecipe = &removeRecipe;
}
