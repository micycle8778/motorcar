#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <stdexcept>
#include <format>
#include <type_traits>

template <typename T>
struct invalid_ptr {
    T& data;
    bool& valid;

    invalid_ptr(T& data, bool& valid) : data(data), valid(valid) {}
};

namespace sol {
    template <typename T>
    struct unique_usertype_traits<invalid_ptr<T>> {
        static T* get(lua_State*, const invalid_ptr<T>& ptr) noexcept {
            return &ptr.data;
        };

        static bool is_null(lua_State*, const invalid_ptr<T>& ptr) noexcept {
            return !ptr.valid;
        }
    };
}

template <typename T>
void sol_lua_check_access(sol::types<T>, lua_State* L, int index, sol::stack::record& tracking) {
    using ObjectType = std::remove_pointer_t<T>;
    
    sol::optional<invalid_ptr<ObjectType>&> maybe_checked =
        sol::stack::check_get<invalid_ptr<ObjectType>&>(L, index, sol::no_panic, tracking);
    
    if (!maybe_checked.has_value()) return;
    if (!maybe_checked->valid) {
        throw std::runtime_error("lua accessed an invalid ptr!");
    }
}

struct State {
    int x = 314;
    int y = 15;

    State() {}
    State(int x, int y) : x(x), y(y) {}

    void print_state() const {
        std::cout << std::format("State(x: {}, y: {})\n", x, y);
    }
};

struct Printer {
    void print_state(const State& s) {
        std::cout << std::format("State(x: {}, y: {})\n", s.x, s.y);
    }
};

int main() {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    State state;
    bool state_valid = true;

    lua.new_usertype<State>("State",
        "x", &State::x,
        "y", &State::y,
        "print_state", &State::print_state
    );

    lua.new_usertype<Printer>("Printer",
        "print_state", &Printer::print_state
    );
    
    sol::object ptr = sol::make_object(lua, invalid_ptr(state, state_valid));
    lua["state"] = invalid_ptr(state, state_valid);
    lua["printer"] = Printer();

    lua.script(R"(
        state:print_state()
        state.x = state.x + 1
    )");

    std::cout << "haii!!!" << std::endl;
    state_valid = false;

    lua.script(R"(
        --state:print_state()
        state.x = state.x + 1
    )");

    return 0;
}
