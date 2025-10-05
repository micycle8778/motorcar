#include "spdlog/common.h"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"
#include "types.h"
#include <ecs.h>
#include <sol/forward.hpp>
#include <string_view>

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

int main(void) {
    spdlog::set_level(spdlog::level::trace);
    motorcar::ECSWorld ecs;

    motorcar::Entity text = ecs.new_entity();
    ecs.emplace_component<Position>(text, Position(100, 100));

    motorcar::Entity player = ecs.new_entity();
    ecs.emplace_component<Position>(player, Position(0, 0));
    ecs.emplace_component<Velocity>(player, Velocity(0, 0));

    motorcar::Entity enemy = ecs.new_entity();
    ecs.emplace_component<Position>(enemy, Position(100, 0));
    ecs.emplace_component<Velocity>(enemy, Velocity(-10, 0));

    auto it = motorcar::ECSWorld::Query<Position, Velocity>::it(ecs);

    // {
    //     Position* pos = ecs.get_component<Position>(text).value();
    //     SPDLOG_TRACE("text.position = ({}, {})", pos->x, pos->y);
    // }
    //
    // {
    //     Position* pos = ecs.get_component<Position>(player).value();
    //     Velocity* vel = ecs.get_component<Velocity>(player).value();
    //     SPDLOG_TRACE("player.position = ({}, {}); player.velocity = ({}, {})", pos->x, pos->y, vel->x, vel->y);
    // }
    //
    // {
    //     Position* pos = ecs.get_component<Position>(enemy).value();
    //     Velocity* vel = ecs.get_component<Velocity>(enemy).value();
    //     SPDLOG_TRACE("enemy.position = ({}, {}); enemy.velocity = ({}, {})", pos->x, pos->y, vel->x, vel->y);
    // }
}
