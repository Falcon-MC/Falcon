#pragma once

#include <memory>

class ItemComponent {
public:
    virtual ~ItemComponent() = default;
    virtual const char *getName() const = 0;
    virtual std::unique_ptr<ItemComponent> clone() const = 0;
};
