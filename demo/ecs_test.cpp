#include "spdlog/common.h"
#include <chrono>
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

template <typename Func>
void bench(const Func func) {
    auto start = std::chrono::steady_clock::now();
    func();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "microseconds: " << elapsed.count() << std::endl;
}

int main(void) {
    spdlog::set_level(spdlog::level::trace);

    const size_t NUM_ENTITIES = 100;
    const size_t TICKS = 1'000'000;
    const size_t BENCHES = 5;

    float sum = 0.;

    for (int bench = 0; bench < BENCHES; bench++) {
        motorcar::ECSWorld ecs;

        for (int idx = 0; idx < NUM_ENTITIES; idx++) {
            motorcar::Entity e;

            auto p = Position(
                ((float)rand() / RAND_MAX) * 100,
                ((float)rand() / RAND_MAX) * 100
            );
            auto v = Velocity(
                -5 + (((float)rand() / RAND_MAX) * 10),
                -5 + (((float)rand() / RAND_MAX) * 10)
            );

            ecs.emplace_component<Position>(e, p);
            ecs.emplace_component<Velocity>(e, v);

            // ecs.insert_component(e, p);
            // ecs.insert_component(e, v);
        }

        auto start = std::chrono::steady_clock::now();
        for (size_t tick = 0; tick < TICKS; tick++) {
            for (auto t : motorcar::Query<Position, Velocity>::it(ecs)) {
                auto p = std::get<Position*>(t);
                auto v = std::get<Velocity*>(t);

                p->x += v->x;
                p->y += v->y;

                sum += p->x;
                sum += p->y;
            }

            ecs.ocean.reset();
        }
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        SPDLOG_TRACE("microseconds: {}; sum: {}", elapsed.count(), sum);
    }
}
