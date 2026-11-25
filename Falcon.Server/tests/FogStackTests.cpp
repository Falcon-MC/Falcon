#include "Core/NBT/Tag.h"
#include "World/Actor/Player/FogStack.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::vector<std::string> idsOf(const FogStack::Entries &stack) {
        std::vector<std::string> ids;
        for (const ServerPlayer::FogEntry &entry: stack)
            ids.push_back(entry.mId);
        return ids;
    }

    void validatesIds() {
        require(FogStack::isValidId("minecraft:fog_crimson_forest"), "a vanilla fog id was refused");
        require(FogStack::isValidId("Minecraft:Fog_Hell"), "ids are not lowercased before the check");
        require(!FogStack::isValidId("fog_hell"), "an id without a namespace was taken");
        require(!FogStack::isValidId("minecraft:"), "an empty name was taken");
        require(!FogStack::isValidId("a:b:c"), "a second colon was taken");
        require(!FogStack::isValidId("minecraft:fog-hell"), "a dash was taken");
    }

    void pushesPopsAndRemoves() {
        FogStack::Entries stack;
        const std::string one = "one";
        const std::string two = "two";
        const std::string three = "three";
        require(FogStack::push(stack, "minecraft:fog_a", one) && FogStack::push(stack, "minecraft:fog_b", two),
                "a push failed");
        require(FogStack::push(stack, "minecraft:fog_c", one), "a third push failed");
        require(FogStack::pop(stack, &one) == "minecraft:fog_c", "pop did not take the topmost entry of that tag");
        require(idsOf(stack) == std::vector<std::string>({"minecraft:fog_a", "minecraft:fog_b"}),
                "pop removed the wrong entry");
        require(FogStack::pop(stack, &three).empty(), "popping an unknown tag succeeded");
        require(FogStack::push(stack, "minecraft:fog_d", two) && FogStack::remove(stack, &two), "remove failed");
        require(idsOf(stack) == std::vector<std::string>({"minecraft:fog_a"}), "remove left an entry of that tag");
        require(!FogStack::remove(stack, &two), "removing a tag twice succeeded");

        FogStack::Entries full;
        for (size_t i = 0; i < FogStack::LIMIT; ++i)
            require(FogStack::push(full, "minecraft:fog_" + std::to_string(i), one), "the stack filled up early");
        require(!FogStack::push(full, "minecraft:fog_extra", one), "pushed past the limit");
    }

    void savesAndLoads() {
        FogStack::Entries stack;
        FogStack::push(stack, "minecraft:fog_a", "one");
        FogStack::push(stack, "minecraft:fog_b", "two");
        Tag data = Tag::ofCompound();
        FogStack::save(stack, data);

        FogStack::Entries loaded;
        FogStack::load(loaded, data);
        require(idsOf(loaded) == idsOf(stack) && loaded[1].mTag == "two", "the stack did not round trip");

        // a script entry with an empty tag stays out of the save; the command entry after it survives
        FogStack::Entries mixed;
        FogStack::push(mixed, "minecraft:fog_hell", "");
        FogStack::push(mixed, "minecraft:fog_b", "two");
        Tag mixedData = Tag::ofCompound();
        FogStack::save(mixed, mixedData);
        FogStack::Entries reloaded;
        FogStack::load(reloaded, mixedData);
        require(idsOf(reloaded) == std::vector<std::string>({"minecraft:fog_b"}),
                "an empty script tag took the later entries with it");

        Tag broken = Tag::ofCompound();
        Tag list = Tag::ofList(Tag::Type::String);
        for (const char *value: {"minecraft:fog_a", "one", "not a fog", "two", "minecraft:fog_c", "three"})
            list.addToList(Tag::ofString(value));
        broken.put("fogCommandStack", std::move(list));
        FogStack::Entries partial;
        FogStack::load(partial, broken);
        require(idsOf(partial) == std::vector<std::string>({"minecraft:fog_a"}),
                "an invalid entry did not end the load");

        Tag odd = Tag::ofCompound();
        Tag oddList = Tag::ofList(Tag::Type::String);
        for (const char *value: {"minecraft:fog_a", "one", "minecraft:fog_b"})
            oddList.addToList(Tag::ofString(value));
        odd.put("fogCommandStack", std::move(oddList));
        FogStack::Entries none;
        FogStack::load(none, odd);
        require(none.empty(), "an odd-length list was loaded");
    }
}

int main() {
    try {
        validatesIds();
        pushesPopsAndRemoves();
        savesAndLoads();
        std::cout << "fog stack checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
