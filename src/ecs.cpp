#include <vector>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#include "ecs.h"

#define MOTORCAR_EAT_EXCEPTION(code, msg) try { code; } catch (const std::exception& e) { SPDLOG_ERROR(msg, " what(): {}", e.what()); } catch (...) { SPDLOG_ERROR(msg); }
using namespace motorcar;

std::vector<std::any> ECSWorld::static_storage;

void ComponentStorage::expand() {
    // realloc with a factor of 1.5x
    size_t new_capacity = capacity < 2 ? 2 : (capacity >> 1) + capacity;

    void* new_blob = malloc(new_capacity * stride);
    for (size_t idx = 0; idx < len; idx++) {
        void* old_ptr = compute_pointer(idx);
        void* new_ptr = (void*)((size_t)new_blob + (idx * stride));

        MOTORCAR_EAT_EXCEPTION(move_from_ptr(new_ptr, old_ptr), "caught exception when moving component");
        MOTORCAR_EAT_EXCEPTION(dtor(old_ptr), "caught exception when destroying moved component");
    }
    free(blob);
    blob = new_blob;

    capacity = new_capacity;
}

void ComponentStorage::insert_sol_object(Entity e, sol::object object) {
    // no MOTORCAR_EAT_EXCEPTION. let it bubble up to lua
    // (this code is already exception safe anyhow)
    if (indices.contains(e)) {
        ctor_from_sol_object(compute_pointer(indices.at(e)), object);
    } else {
        if (len == capacity) {
            expand();
        }

        ctor_from_sol_object(compute_pointer(len), object);

        len++;
        indices.emplace(e, len - 1);
        entities.push_back(e);
    }
}

bool ComponentStorage::has_component(Entity e) {
    return indices.contains(e);
}

sol::object ComponentStorage::get_component_as_lua_object(Entity e, sol::state& lua) {
    if (!indices.contains(e)) {
        return sol::nil;
    }

    return get_sol_object(compute_pointer(indices.at(e)), lua);
}

void ComponentStorage::remove_component(Entity e) {
    if (!indices.contains(e)) {
        SPDLOG_TRACE("told to remove a component on an entity that doesn't have it...");
        return;
    }

    size_t index = indices.at(e);

    // destroy the component
    void* ptr = compute_pointer(index);
    MOTORCAR_EAT_EXCEPTION(dtor(ptr), "caught unknown exception removing component");

    if (index != len - 1) {
        // move the component
        void* last_ptr = compute_pointer(len - 1);
        MOTORCAR_EAT_EXCEPTION(move_from_ptr(ptr, last_ptr), "caught unknown exception moving component");
        MOTORCAR_EAT_EXCEPTION(dtor(last_ptr), "caught unkown exception destroying moved component");

        // update the metadata
        Entity last_e = entities[len - 1];
        indices.emplace(last_e, index);

        entities[index] = last_e;
    }

    indices.erase(e);
    entities.pop_back();
    len--;
}

ComponentStorage::~ComponentStorage() {
    if (blob == nullptr) return;

    for (size_t idx = 0; idx < len; idx++) {
        dtor(compute_pointer(idx));
    }

    free(blob);
}
