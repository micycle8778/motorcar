// BUG: the usage of svhasher falsely assumes that the hashes will never collide
// there hasn't been a hash collision yet, but it will be annoying when it happens

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

#include "spdlog/spdlog.h"

#include "resources.h"

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

void ResourceManager::watch_file(std::filesystem::path file_path, std::function<void()> callback) {
// do nothing for emscripten
#ifdef __EMSCRIPTEN__
#else
    std::thread thread {[=]() {
#ifdef __linux__
        int fd = inotify_init();
        if (fd < 0) {
            SPDLOG_ERROR("inotify_init failed. errno: {}, strerror: {}", errno, strerror(errno));
            return;
        }

        int wd = inotify_add_watch(fd, file_path.c_str(), IN_MODIFY);
        if (wd < 0) {
            SPDLOG_ERROR("inotify_add_watch failed. errno: {}, strerror: {}", errno, strerror(errno));
            return;
        }

        char buffer[1024];
        while (true) {
            int length = read(fd, buffer, sizeof(buffer));
            if (length > 0) {
                callback();
            }
            inotify_add_watch(fd, file_path.c_str(), IN_MODIFY);
        }
#elif _WIN32
        try {
            // Start by noting the last write time. If the file doesn't exist yet,
            // last_write_time will throw; swallow that and retry until it exists.
            std::error_code ec;
            auto last_write = std::filesystem::last_write_time(file_path, ec);
            if (ec) {
                // file may not exist yet - keep looping until it does
                while (true) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    last_write = std::filesystem::last_write_time(file_path, ec);
                    if (!ec) break;
                }
            }

            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));

                std::error_code ec2;
                auto now_write = std::filesystem::last_write_time(file_path, ec2);
                if (ec2) {
                    // if the file was removed or is otherwise unreadable, just wait
                    continue;
                }

                if (now_write != last_write) {
                    last_write = now_write;
                    try {
                        callback();
                    } catch (const std::exception& e) {
                        SPDLOG_ERROR("watch_file callback threw exception: {}", e.what());
                    } catch (...) {
                        SPDLOG_ERROR("watch_file callback threw unknown exception");
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            SPDLOG_ERROR("watch_file failed: {}", e.what());
            return;
        }
#else
    #warning "No hot reload handling!"
#endif
    }};
    thread.detach();
#endif
}

bool ResourceManager::load_resource(std::string_view resource_path) {
   // maybe we loaded it already?
    if (path_to_resource_map.contains(svhasher{}(resource_path))) {
        return true;
    }

    if (is_virtual_filename(resource_path)) {
        return false;
    }

    // loop through every resource, trying to load the damn thing
    std::filesystem::path real_path = convert_path(resource_path);
    std::ifstream file_stream(real_path, std::ios::binary);

    if (file_stream.fail()) {
        spdlog::error("Couldn't open file backing resource '{}'", resource_path);
        return {};
    }

    for (auto& loader : resource_loaders) {
        std::optional<Resource> maybe_resource = loader->load_resource(real_path, file_stream, resource_path);
        if (maybe_resource.has_value()) {
            path_to_resource_map.insert(std::make_pair(svhasher{}(resource_path), std::move(*maybe_resource)));
            return true;
        }
    }
    
    // failure :(
    return false;
}

void ResourceManager::insert_resource(std::string_view resource_path, Resource&& resource) {
    if (!is_virtual_filename(resource_path)) {
        SPDLOG_WARN("Virtual resource filenames should start with '::'.");
    }
    
    path_to_resource_map.insert({ svhasher{}(resource_path), std::move(resource) /* ??? how tf does std::move-ing an rvalue work ??? */ });
}

void ResourceManager::register_resource_loader(std::unique_ptr<ILoadResources> resource_loader) {
    resource_loaders.push_back(std::move(resource_loader));
}
