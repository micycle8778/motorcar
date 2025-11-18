#pragma once

#include <memory>
#include <string_view>
#include <webgpu.h>

struct GLFWwindow;

namespace motorcar {
    const int DEFAULT_WIDTH  = 1280/2;
    const int DEFAULT_HEIGHT = 720/2;

    struct Engine;

    class GraphicsManager {
        public:
            struct WebGPUState;

        private:
            friend class InputManager; // friend :)

            GLFWwindow* window;
            std::shared_ptr<WebGPUState> webgpu;
            Engine& engine;
            void draw_sprites(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
            void draw_text(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
            void draw_3d(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
            void draw_sprite_3d(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
            void draw_colliders(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
            void clear_screen(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);

        public:
            GraphicsManager(
                    Engine& engine, 
                    const std::string_view& window_name, 
                    int window_width = DEFAULT_WIDTH, 
                    int window_height = DEFAULT_HEIGHT
            );
            bool window_should_close();
            void draw();


            GraphicsManager(GraphicsManager&) = delete;
            GraphicsManager& operator=(GraphicsManager&) = delete;

            ~GraphicsManager();
    };
}
