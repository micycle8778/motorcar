#include "resources.h"
#include "soloud.h"
#include "soloud_wav.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <string_view>

using namespace motorcar;

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

void ResourceManager::register_resource_loader(std::unique_ptr<ILoadResources> resource_loader) {
    resource_loaders.push_back(std::move(resource_loader));
}
