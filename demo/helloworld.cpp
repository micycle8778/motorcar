#include <sol/raii.hpp>
#include <sol/types.hpp>

#include <spdlog/spdlog.h>

#include <engine.h>
#include <sound.h>
#include <input.h>
#include <types.h>
#include <scripts.h>
#include <ecs.h>
#include <components.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::trace);

    motorcar::Engine e("helloworld");

    e.run();

    return 0;
}
