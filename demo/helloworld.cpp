#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <sol/forward.hpp>
#include <vector>

// #define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

struct Vec2 {
    float x;
    float y;

    Vec2(float x, float y) : x(x), y(y) {}
};

/*

local wrapped_metatable = {
    __index = function(t, key)
        if t.__is_valid then
            return t.__interior[key]
        end

        return nil
    end,

    -- TODO: passthrough the rest of the metamethods
}

local Wrapped = {
    new = function(interior)
        -- TODO: check interior is table or userdata
        return setmetatable({
            __is_valid = true,
            __interior = interior,
        }, wrapped_metatable)
    end,
}

local boss_pos = nil
ECS.for_each({ "boss", "position" }, function(_boss, pos)
    boss_pos = pos -- now i have the boss' position :D
end)

ECS.remove_entity(ECS.get({ "_entity", "boss" })[0])
print(boss_pos) -- what now?

*/

#define NUM_VECS 200000

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
    std::vector<Vec2> vs;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
    }

    float sum = 0.f;
    auto translate = [&](Vec2& v) {
        v.x += 2;
        v.y += 2;

        sum += v.x;
        sum += v.y;
    };

    std::cout << "native: ";
    bench([&](){
        for (Vec2& v : vs) {
            translate(v);
        }
    });
    lua["sum"] = lua["sum"].get<float>() + sum;
}

void cpp_func_lua_mem(sol::state& lua) {
    std::vector<sol::table> vs;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        float x = (float)rand() / RAND_MAX;
        float y = (float)rand() / RAND_MAX;

        auto v = lua.create_table();
        v["x"] = x;
        v["y"] = y;

        vs.push_back(v);
    }

    float sum = 0.f;
    auto translate = [&](sol::table& v) {
        v["x"] = v["x"].get<float>() + 2;
        v["y"] = v["y"].get<float>() + 2;

        sum += v["x"].get<float>();
        sum += v["y"].get<float>();
    };

    std::cout << "cpp_func_lua_mem: ";
    bench([&](){
        for (sol::table& v : vs) {
            translate(v);
        }
    });
    lua["sum"] = lua["sum"].get<float>() + sum;
}

void cpp_mem_lua_func(sol::state& lua) {
    std::vector<Vec2> vs;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
    }

    sol::function translate = lua["translate"];

    std::cout << "cpp_mem_lua_func: ";
    bench([&](){
        for (Vec2& v : vs) {
            translate(&v);
        }
    });
}

void cpp_mem_lua_func_aot(sol::state& lua) {
    std::vector<Vec2> vs;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        vs.push_back(Vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));
    }

    std::vector<sol::object> lua_ready;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        auto& v = vs[idx];
        lua_ready.push_back(sol::make_object(lua, &v));
    }

    sol::function translate = lua["translate"];

    std::cout << "cpp_mem_lua_func_aot: ";
    bench([&](){
        for (sol::object& v : lua_ready) {
            translate(v);
        }
    });
}

void cpp_glue_lua_all(sol::state& lua) {
    std::vector<sol::table> vs;
    sol::function translate = lua["translate"];
    vs.reserve(NUM_VECS); // we're not measuring the time to expand the vector

    std::cout << "cpp_glue_lua_all: ";
    bench([&](){
        for (int idx = 0; idx < NUM_VECS; idx++) {
            float x = (float)rand() / RAND_MAX;
            float y = (float)rand() / RAND_MAX;

            auto v = lua.create_table();
            v["x"] = x;
            v["y"] = y;

            vs.push_back(v);
        }

        for (sol::table& v : vs) {
            translate(v);
        }
    });
}

void cpp_glue_lua_all_aot(sol::state& lua) {
    std::vector<sol::table> vs;
    for (int idx = 0; idx < NUM_VECS; idx++) {
        float x = (float)rand() / RAND_MAX;
        float y = (float)rand() / RAND_MAX;

        auto v = lua.create_table();
        v["x"] = x;
        v["y"] = y;

        vs.push_back(v);
    }

    sol::function translate = lua["translate"];

    std::cout << "cpp_glue_lua_all_aot: ";
    bench([&](){
        for (sol::table& v : vs) {
            translate(v);
        }
    });
}

int main( int argc, const char* argv[] ) {
    sol::state lua;
    lua.new_usertype<Vec2>("Vec2",
        "x", &Vec2::x,
        "y", &Vec2::y
    );
    lua.script(R"(
        sum = 0
        function translate(v)
            v.x = v.x + 2
            v.y = v.y + 2

            sum = sum + v.x
            sum = sum + v.y
        end
    )");

    // Vec2 my_vec(1., 2.1);
    // sol::object lua_ready = sol::make_object(lua, &my_vec);
    //
    // std::cout << my_vec.x << " " << my_vec.y << std::endl;
    // lua["translate"](lua_ready);
    // std::cout << my_vec.x << " " << my_vec.y << std::endl;
    //
    // return 0;

    repeat([&](){ native_bench(lua); }, 5);
    std::cout << std::endl;

    repeat([&](){ cpp_func_lua_mem(lua); }, 5);
    std::cout << std::endl;

    repeat([&](){ cpp_mem_lua_func(lua); }, 5);
    std::cout << std::endl;

    repeat([&](){ cpp_mem_lua_func_aot(lua); }, 5);
    std::cout << std::endl;

    repeat([&](){ cpp_glue_lua_all(lua); }, 5);
    std::cout << std::endl;

    repeat([&](){ cpp_glue_lua_all_aot(lua); }, 5);
    std::cout << std::endl;

    return 0;
}
