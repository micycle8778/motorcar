#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>
#include "spdlog/spdlog.h"

// TODO:
// resource manager needs to
// 1. convert paths "file.txt" to "cwd://assets/file.txt"
// 2. pick the correct resource loader to load these resources
//    (by magic number or file extension)
// 3. automatically reload resources when changed
// 4. make available these various resources

namespace motorcar {
    // type erased container
    // cannot be copied, but can be moved
    class Resource {
        void* data = nullptr;
        const std::type_info* stored_type = nullptr;
        void (*dtor)(void*) = nullptr;

        public:
            Resource() = default;
            template <typename T>
            Resource(T&& raw_resource) {
                data = new T(std::forward<T>(raw_resource));
                stored_type = &typeid(T);
                dtor = [](void* ptr) { delete static_cast<T*>(ptr); };
            }

            template <typename T>
            std::optional<T*> get() {
                if (data != nullptr && *stored_type == typeid(T)) {
                    return static_cast<T*>(data);
                } else {
                    return {};
                }
            }

            Resource(const Resource&) = delete;
            Resource& operator=(const Resource&) = delete;

            Resource(Resource&& other) noexcept;
            Resource& operator=(Resource&& other) noexcept;

            ~Resource(); 
    };

    class ILoadResources {
        public:
            static std::vector<uint8_t> get_data_from_file_stream(std::ifstream& file_stream);
            virtual std::optional<Resource> load_resource(const std::filesystem::path& path, std::ifstream& file_stream) = 0;

            virtual ~ILoadResources() = default;
    };

    class ResourceManager {
        std::unordered_map<std::string_view, Resource> path_to_resource_map;
        std::vector<std::unique_ptr<ILoadResources>> resource_loaders;

        public:
            ResourceManager();
            void register_resource_loader(std::unique_ptr<ILoadResources> resource_loader);
            static std::filesystem::path convert_path(std::string_view path);

            template <typename T>
            std::optional<T*> get_resource(std::string_view file_name);
    };
}

template <typename T>
std::optional<T*> motorcar::ResourceManager::get_resource(std::string_view filename) {
    // look for T in `path_to_resource_map`
    if (path_to_resource_map.contains(filename)) {
        auto maybe = path_to_resource_map[filename].get<T>();
        if (maybe.has_value()) {
            return maybe;
        } else {
            return {};
        }
    }

    // if none found, find a good resource loader
    std::filesystem::path path = convert_path(filename);
    std::ifstream file_stream(path, std::ios::binary);

    if (file_stream.fail()) {
        spdlog::error("Couldn't open file backing resource '{}'", filename);
        return {};
    }

    for (auto& loader : resource_loaders) {
        std::optional<Resource> maybe_resource = loader->load_resource(path, file_stream);

        // if found, call this function again
        if (maybe_resource.has_value()) {
            path_to_resource_map.insert(std::make_pair(filename, std::move(*maybe_resource)));
            return get_resource<T>(filename);
        }
    }

    // if not found, return none
    spdlog::error("Couldn't load resource '{}'", filename);
    return {};
}
