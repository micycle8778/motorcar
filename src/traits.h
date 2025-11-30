#pragma once

#include <string_view>

namespace motorcar {
    template <typename T>
    struct ComponentTypeTrait {
        constexpr static bool value = false;
        constexpr static std::string_view component_name = "";
    };
}
