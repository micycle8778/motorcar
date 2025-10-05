#include <filesystem>
#include <fstream>
#include <optional>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <type_traits>

#include <spdlog/spdlog.h>

#include "types.h"

// resource manager needs to
// 1. convert paths "file.txt" to "cwd://assets/file.txt"
// 2. pick the correct resource loader to load these resources
//    (by magic number or file extension)
// 3. make available these various resources
// 4. TODO: automatically reload resources when changed

namespace motorcar {
    // type erased version of unique_ptr for storing resources
    class Resource {
        void* data = nullptr;
        const std::type_info* stored_type = nullptr;
        void (*dtor)(void*) = nullptr;

        public:
            Resource() = default;
            template <typename T>
            Resource(T&& t) {
                static_assert(!std::is_same<std::remove_reference_t<T>, Resource>::value, "T cannot be Resource itself");
                data = new std::remove_reference_t<T>(std::forward<T>(t));
                stored_type = &typeid(std::remove_reference_t<T>);
                dtor = [](void* ptr) { delete static_cast<std::remove_reference_t<T>*>(ptr); };
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
            static std::vector<u8> get_data_from_file_stream(std::ifstream& file_stream);
            virtual std::optional<Resource> load_resource(const std::filesystem::path& path, std::ifstream& file_stream, std::string_view resource_path) = 0;

            virtual ~ILoadResources() = default;
    };

    class ResourceManager {
        typedef std::hash<std::string_view> svhasher;
        // std::hash<std::string_view>() -> size_t
        std::unordered_map<size_t, Resource> path_to_resource_map;
        std::vector<std::unique_ptr<ILoadResources>> resource_loaders;

        public:
            void register_resource_loader(std::unique_ptr<ILoadResources> resource_loader);
            static std::filesystem::path convert_path(std::string_view path);

            template <typename T>
            std::optional<T*> get_resource(std::string_view resource_path);
            bool load_resource(std::string_view resource_path);
            void insert_resource(std::string_view resource_path, Resource&& resource);
    };
}

template <typename T>
std::optional<T*> motorcar::ResourceManager::get_resource(std::string_view resource_path) {
    if (!load_resource(resource_path)) {
        spdlog::error("Couldn't load resource '{}'", resource_path);
        return {};
    }
    
    // look for T in `path_to_resource_map`
    if (path_to_resource_map.contains(svhasher{}(resource_path))) {
        auto maybe = path_to_resource_map[svhasher{}(resource_path)].get<T>();
        if (maybe.has_value()) {
            return maybe;
        } else {
            return {};
        }
    } else [[unlikely]] {
        // unreachable
        std::abort();
    }
}
