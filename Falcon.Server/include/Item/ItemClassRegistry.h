#pragma once

#include "Item/Item.h"

#include <memory>
#include <string>

class ItemClassRegistry {
public:
    using Matcher = bool (*)(const std::string &);
    using Factory = std::unique_ptr<Item> (*)(const Item &);

    struct Registration {
        Registration(int priority, Matcher matcher, Factory factory);
    };

    static std::unique_ptr<Item> create(const Item &item);
};

#define FALCON_REGISTER_ITEM(type, priority)                                             \
    static const ItemClassRegistry::Registration gItemRegistration##type(                \
            (priority), &type::matches,                                                  \
            [](const Item &item) -> std::unique_ptr<Item> {                              \
                return std::make_unique<type>(item);                                     \
            })

#define FALCON_REGISTER_ITEM_CUSTOM(tag, priority, matcher, factory)                     \
    static const ItemClassRegistry::Registration gItemRegistration##tag(                 \
            (priority), (matcher), (factory))
