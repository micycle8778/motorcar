#include <string>
#include <filesystem>

// TODO:
// resource manager needs to
// 1. convert paths "file.txt" to "cwd://assets/file.txt"
// 2. pick the correct resource loader to load these resources
//    (by magic number or file extension)
// 3. automatically reload resources when changed
// 4. make available these various resources
//    (both the parsed and unparsed versions)

namespace motorcar {
    struct Engine;

    class IResourceLoader {
    };

    class ResourceManager {
        Engine& engine;

        static constexpr std::filesystem::path convert_path(const std::string_view& path);

        public:
            ResourceManager(Engine& engine);
    };
}
