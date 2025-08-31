#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <sol/forward.hpp>
#include <sol/object.hpp>
#include <sol/unique_usertype_traits.hpp>
#include <stdexcept>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

struct Vec2 {
    float x;
    float y;

    Vec2(float x, float y) : x(x), y(y) {}
};

struct invalid_ptr_dereference : std::runtime_error {
    invalid_ptr_dereference() : std::runtime_error("invalid invalid_ptr dereference") {}
};
template <typename T>
struct invalid_ptr {
    T* interior;
    bool valid = true;

    [[nodiscard]] constexpr bool is_valid() const { return valid; }
    void invalidate() { valid = false; }

    [[nodiscard]] constexpr T* get() const { 
        if (!valid) throw invalid_ptr_dereference();
        return interior;
    }
    [[nodiscard]] constexpr T& operator*() const { return *get(); }
    [[nodiscard]] constexpr T* operator->() const { return get(); }

    
};

namespace sol {
    template <typename T>
    struct unique_usertype_traits<invalid_ptr<T>> {
        static auto* get(lua_State*, const auto& ptr) noexcept {
			return ptr.get();
		}

		static bool is_null(lua_State*, const auto& ptr) noexcept {
			return !ptr.is_valid() || ptr.get() == nullptr;
		}
    };
}

template <typename T>
void sol_lua_check_access(sol::types<T>, lua_State* L, int index, sol::stack::record& tracking) {
	sol::optional<invalid_ptr<T>&> maybe_checked = sol::stack::check_get<invalid_ptr<T>&>(L, index, sol::no_panic, tracking);
	if (!maybe_checked.has_value()) {
		return;
	}

	invalid_ptr<T>& checked = *maybe_checked;
	if (!checked.is_valid()) {
		// freak out in whatever way is appropriate, here
		throw std::runtime_error("You dun goofed");
	}
}

// number of 2d vectors to translate
#define NUM_VECS 20000
// number of times to translate the vectors
// think of this as the number of ticks run by the engine
#define INNER_RUNS 1000
// number of times the benchmark is run
#define OUTER_RUNS 5

template <typename Func>
void bench(const Func func) {
    auto start = std::chrono::steady_clock::now();
    func();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "microseconds: " << elapsed.count() << std::endl;
}

template <typename Func>
void repeat(const Func func, int times) {
    for (; times > 0; times--)
        func();
}

void native_bench(sol::state& lua) {
    float sum = 0.f;
    auto translate = [&](Vec2& v) {
        v.x += 2;
        v.y += 2;

        sum += v.x;
        sum += v.y;
    };

    std::cout << "native: ";
    bench([&](){
        std::vector<Vec2> vs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
        }

        repeat([&]() {
            for (Vec2& v : vs) {
                translate(v);
            }
        }, INNER_RUNS);
    });

    lua["sum"] = lua["sum"].get<float>() + sum;
}

void cpp_func_lua_mem(sol::state& lua) {
    float sum = 0.f;
    auto translate = [&](sol::table& v) {
        v["x"] = v["x"].get<float>() + 2;
        v["y"] = v["y"].get<float>() + 2;

        sum += v["x"].get<float>();
        sum += v["y"].get<float>();
    };

    std::cout << "cpp_func_lua_mem: ";
    bench([&](){
        std::vector<sol::table> vs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            float x = (float)rand() / RAND_MAX;
            float y = (float)rand() / RAND_MAX;

            auto v = lua.create_table();
            v["x"] = x;
            v["y"] = y;

            vs.push_back(v);
        }

        repeat([&]() {
            for (sol::table& v : vs) {
                translate(v);
            }
        }, INNER_RUNS);
    });
    lua["sum"] = lua["sum"].get<float>() + sum;
}

void cpp_mem_lua_func(sol::state& lua) {
    sol::function translate = lua["translate"];

    std::cout << "cpp_mem_lua_func: ";
    bench([&](){
        std::vector<Vec2> vs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
        }
        repeat([&]() {
            for (Vec2& v : vs) {
                translate(&v);
            }
        }, INNER_RUNS);
    });
}

void cpp_mem_lua_func_write_back(sol::state& lua) {
    sol::function translate = lua["translate"];

    std::cout << "cpp_mem_lua_func_write_back: ";
    bench([&](){
        std::vector<Vec2> vs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
        }
        repeat([&]() {
            for (Vec2& v : vs) {
                sol::object obj = sol::make_object(lua, v);
                translate(obj);
                v = obj.as<Vec2>();
            }
        }, INNER_RUNS);
    });
}

void cpp_mem_lua_func_make_object(sol::state& lua) {
    sol::function translate = lua["translate"];

    std::cout << "cpp_mem_lua_func_make_object: ";
    bench([&](){
        std::vector<Vec2> vs;
        std::vector<sol::object> objs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
        }
        for (int idx = 0; idx < NUM_VECS; idx++) {
            objs.push_back(sol::make_object(lua, &vs[idx]));
        }

        repeat([&]() {
            for (sol::object& v : objs) {
                translate(v);
            }
        }, INNER_RUNS);
    });
}

void cpp_glue_lua_all(sol::state& lua) {
    sol::function translate = lua["translate"];

    std::cout << "cpp_glue_lua_all: ";
    bench([&](){
        std::vector<sol::table> vs;
        for (int idx = 0; idx < NUM_VECS; idx++) {
            float x = (float)rand() / RAND_MAX;
            float y = (float)rand() / RAND_MAX;

            auto v = lua.create_table();
            v["x"] = x;
            v["y"] = y;

            vs.push_back(v);
        }

        repeat([&]() {
            for (sol::table& v : vs) {
                translate(v);
            }
        }, INNER_RUNS);
    });
}

int main( int argc, const char* argv[] ) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::package);
    lua.new_usertype<Vec2>("Vec2",
        "x", &Vec2::x,
        "y", &Vec2::y
    );
    lua.script(R"(
        last_v = nil
        sum = 0
        function translate(v)
            v.x = v.x + 2
            v.y = v.y + 2

            sum = sum + v.x
            sum = sum + v.y

            if last_v ~= nil then
                print(last_v.x)
            end
            if last_v ~= nil then
                print(last_v.y)
            end

            last_v = v
        end
    )");

    Vec2 v1(1.0, 1.0);
    Vec2 v2(2.0, 2.0);

    invalid_ptr<Vec2> ptr(&v1);

    lua["translate"](ptr);
    lua["last_v"].get<invalid_ptr<Vec2>>().invalidate();
    std::cout << lua["last_v"].get<invalid_ptr<Vec2>>().is_valid() << std::endl;
    lua["translate"](v2);

    return 0;

    // Vec2 my_vec(1., 2.1);
    // sol::object lua_ready = sol::make_object(lua, &my_vec);
    //
    // std::cout << my_vec.x << " " << my_vec.y << std::endl;
    // lua["translate"](lua_ready);
    // std::cout << my_vec.x << " " << my_vec.y << std::endl;
    //
    // return 0;

    repeat([&](){ native_bench(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    repeat([&](){ cpp_func_lua_mem(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    repeat([&](){ cpp_mem_lua_func(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    repeat([&](){ cpp_mem_lua_func_make_object(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    repeat([&](){ cpp_mem_lua_func_write_back(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    repeat([&](){ cpp_glue_lua_all(lua); }, OUTER_RUNS);
    std::cout << std::endl;

    return 0;
}
