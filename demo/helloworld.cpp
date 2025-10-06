#include <sol/raii.hpp>
#include <sol/types.hpp>
#include <vector>

#include <spdlog/spdlog.h>

#include <engine.h>
#include <sound.h>
#include <input.h>
#include <types.h>
#include <scripts.h>
#include <ecs.h>

// lifted from ecs_test.cpp
struct Position {
    float x;
    float y;

    Position(float x, float y) : x(x), y(y) {}
    Position(sol::object object) {
        *this = object.as<Position>();
    }
};

struct Velocity {
    float x;
    float y;

    Velocity(float x, float y) : x(x), y(y) {}
    Velocity(sol::object object) {
        *this = object.as<Velocity>();
    }
};

template <>
struct motorcar::ComponentTypeTrait<Position> {
    constexpr static bool value = true;
    constexpr static std::string_view component_name = "position";
};

template <>
struct motorcar::ComponentTypeTrait<Velocity> {
    constexpr static bool value = true;
    constexpr static std::string_view component_name = "velocity";
};

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::trace);

    int press_count = 0;
    int release_count = 0;

    motorcar::Engine e("helloworld", {
        motorcar::Sprite {
            .position = motorcar::vec2(0),
            .scale = motorcar::vec2(25),
            // .scale = motorcar::vec2(0.1, 0.1),
            .depth = .3,
            .texture_path = "bowser.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(30),
            .scale = motorcar::vec2(45),
            .depth = .1,
            .texture_path = "watercan.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(-50),
            .scale = motorcar::vec2(16),
            .depth = .2,
            .texture_path = "michael-day.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(-25, -20),
            .scale = motorcar::vec2(32),
            .depth = .2,
            .texture_path = "insect.png"
        },
    });

    e.scripts->lua.new_usertype<Position>("Position",
        sol::constructors<Position(float, float)>(),
        "x", &Position::x,
        "y", &Position::y,
        sol::meta_function::to_string, [](Position& p) { return std::format("({}, {})", p.x, p.y); }
    );

    e.scripts->lua.new_usertype<Velocity>("Velocity",
        sol::constructors<Velocity(float, float)>(),
        "x", &Velocity::x,
        "y", &Velocity::y,
        sol::meta_function::to_string, [](Velocity& p) { return std::format("({}, {})", p.x, p.y); }
    );

    auto player = e.ecs->new_entity();
    e.ecs->emplace_native_component<Position>(player, 0, 0);
    e.ecs->emplace_native_component<Velocity>(player, 0, 0);

    auto enemy = e.ecs->new_entity();
    e.ecs->emplace_native_component<Position>(enemy, 100, 0);
    e.ecs->emplace_native_component<Velocity>(enemy, -10, 0);

    e.ecs->flush_command_queue();

    e.scripts->run_script("first.lua");
    e.run([&]() {
        e.scripts->run_script("hello.lua");
    });

    return 0;
}
