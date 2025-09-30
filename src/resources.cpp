#include "resources.h"
#include "soloud.h"
#include "soloud_wav.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <string_view>

using namespace motorcar;

namespace {
    bool is_virtual_filename(std::string_view filename) {
        return filename.starts_with("::");
    }
}

Resource::Resource(Resource&& other) noexcept {
    if (this != &other) {
        data = other.data;
        dtor = other.dtor;
        stored_type = other.stored_type;

        other.data = nullptr;
    }
}

Resource& Resource::operator=(Resource&& other) noexcept {
    if (this != &other) {
        if (data != nullptr) {
            dtor(data);
        }

        data = other.data;
        dtor = other.dtor;
        stored_type = other.stored_type;

        other.data = nullptr;
    }

    return *this;
}

Resource::~Resource() {
    if (data != nullptr) {
        dtor(data);
    }
}

std::vector<uint8_t> ILoadResources::get_data_from_file_stream(std::ifstream& file_stream) {
    file_stream.seekg(0, std::ios::end);
    std::streamsize size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> result(size);
    file_stream.read(reinterpret_cast<char*>(result.data()), size);

    return result;
}

std::filesystem::path ResourceManager::convert_path(std::string_view path) {
    auto cwd = std::filesystem::current_path();
    auto assets_path = cwd / "assets";

    // TODO: ensure convert_path doesn't return a path outside
    // of assets_path
    return assets_path / path;
}

bool motorcar::ResourceManager::load_resource(std::string_view filename) {
   // maybe we loaded it already?
    if (path_to_resource_map.contains(filename)) {
        return true;
    }

    if (is_virtual_filename(filename)) {
        return false;
    }

    // loop through every resource, trying to load the damn thing
    std::filesystem::path path = convert_path(filename);
    std::ifstream file_stream(path, std::ios::binary);

    if (file_stream.fail()) {
        spdlog::error("Couldn't open file backing resource '{}'", filename);
        return {};
    }

    for (auto& loader : resource_loaders) {
        std::optional<Resource> maybe_resource = loader->load_resource(path, file_stream);
        if (maybe_resource.has_value()) {
            path_to_resource_map.insert(std::make_pair(filename, std::move(*maybe_resource)));
            return true;
        }
    }
    
    // failure :(
    return false;
}

void motorcar::ResourceManager::insert_resource(std::string_view filename, Resource&& resource) {
    if (!is_virtual_filename(filename)) {
        spdlog::warn("Virtual resource filenames should start with '::'.");
    }
    
    path_to_resource_map.insert({ filename, std::move(resource) /* ??? how tf does std::move-ing an rvalue work ??? */ });
}

void ResourceManager::register_resource_loader(std::unique_ptr<ILoadResources> resource_loader) {
    resource_loaders.push_back(std::move(resource_loader));
}
