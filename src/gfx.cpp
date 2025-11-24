#include <memory>
#include <sol/raii.hpp>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <cstddef>
#include <optional>
#include <ranges>
#include <algorithm>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include "assimp/material.h"
#include "glm/geometric.hpp"
#include "webgpu/webgpu.h"
#include <glfw3webgpu.h>
#include <sol/types.hpp>

#include <spdlog/spdlog.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <incbin.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "gfx.h"
#include "resources.h"
#include "engine.h"
#include "types.h"
#include "components.h"
#include "scripts.h"
#include "physics3d.h"

#define WGPU_STRING_VIEW(label) WGPUStringView(label, strlen(label))

using namespace motorcar;

INCTXT(pipeline_2d_shader, "pipeline2d.wgsl");
INCTXT(pipeline_3d_shader, "pipeline3d.wgsl");
INCTXT(pipeline_colliders_shader, "pipeline_colliders.wgsl");
INCBIN(liberation_sans, "LiberationSans-Regular.ttf");

const std::string_view MISSING_TEXTURE_PATH = "::gfx::missing_texture";
const std::string_view BLANK_TEXTURE_PATH = "::gfx::blank_texture";

struct GraphicsManager::WebGPUState {
    WGPUInstance instance = nullptr;
    WGPUSurface surface = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;

    WGPUBuffer cube_vertex_buffer = nullptr;
    WGPUBuffer uniforms_buffer_3d = nullptr;
    WGPUShaderModule shader_module_3d = nullptr;
    WGPUShaderModule shader_module_colliders = nullptr;
    WGPURenderPipeline pipeline_3d = nullptr;
    WGPURenderPipeline pipeline_colliders = nullptr;
    WGPUTexture depth_texture = nullptr;
    WGPUBindGroupLayout layout_3d = nullptr;
    WGPUBindGroupLayout layout_colliders = nullptr;
    WGPUBindGroup bind_group_colliders = nullptr;

    WGPUBuffer quad_vertex_buffer = nullptr;
    WGPUBuffer uniforms_buffer_2d = nullptr;
    WGPUSampler sampler = nullptr;
    WGPUShaderModule shader_module_2d = nullptr;
    WGPURenderPipeline pipeline_2d = nullptr;
    WGPUBindGroupLayout layout_2d = nullptr;

    WGPUBuffer light_buffer = nullptr;

    WebGPUState(GLFWwindow*);

    void configure_surface(GLFWwindow* window);
    void setup(GLFWwindow*);
    void init_pipeline_2d();
    void init_pipeline_3d();
};

namespace {
    template< typename T > constexpr const T* to_ptr( const T& val ) { return &val; }
    template< typename T, std::size_t N > constexpr const T* to_ptr( const T (&&arr)[N] ) { return arr; }

    glm::mat4 to_glm(const aiMatrix4x4& aiMat) {
        return glm::mat4(
                aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
                aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
                aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
                );
    }

    struct InstanceData2D {
        vec3 translation;
        vec2 scale;
        //float rotation;
    };

    struct InstanceData3D {
        mat4 model;
        // mat3x3f's memory layout is deranged
        vec4 n0;
        vec4 n1;
        vec4 n2;
        vec4 albedo;
    };

    struct Uniforms2D {
        mat4 projection;
    };

    struct VertexData2D {
        float x, y;
        float u, v;
    };

    struct VertexData3D {
        float x, y, z;
        float u, v;
        float nx, ny, nz;
    };

    struct Uniforms3D {
        mat4 view_projection;
        vec3 camera_pos;
        f32 _padding;
    };

    constexpr u32 NUM_LIGHTS = 8;
    struct LightData {
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;

        vec3 position;
        f32 distance;
    };

    WGPUTextureFormat wgpuSurfaceGetPreferredFormat( WGPUSurface surface, WGPUAdapter adapter ) {
        WGPUSurfaceCapabilities capabilities{};
        wgpuSurfaceGetCapabilities( surface, adapter, &capabilities );
        const WGPUTextureFormat result = capabilities.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers( capabilities );
        return result;
    }

    struct Texture {
        WGPUTexture data = nullptr;
        WGPUBindGroup bind_group_2d = nullptr;
        WGPUBindGroup bind_group_3d = nullptr;
        WGPUTextureView texture_view = nullptr;
        size_t width = 0;
        size_t height = 0;

        void invalidate() {
            texture_view = nullptr;
            bind_group_2d = nullptr;
            bind_group_3d = nullptr;
            data = nullptr;
        }

        void destroy() {
            if (bind_group_2d != nullptr) wgpuBindGroupRelease(bind_group_2d);
            if (bind_group_3d != nullptr) wgpuBindGroupRelease(bind_group_3d);
            if (texture_view != nullptr) wgpuTextureViewRelease(texture_view);
            if (data != nullptr) wgpuTextureDestroy(data);

            invalidate();
        }

        Texture(GraphicsManager::WebGPUState& webgpu, const u8* image_data, size_t width, size_t height, std::string_view label) {
            this->width = width;
            this->height = height;

            data = wgpuDeviceCreateTexture(webgpu.device, to_ptr(WGPUTextureDescriptor{
                .label = WGPUStringView { .data = label.data(), .length = label.size() },
                .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
                .dimension = WGPUTextureDimension_2D,
                .size = { (uint32_t)width, (uint32_t)height, 1 },
                .format = WGPUTextureFormat_RGBA8UnormSrgb,
                .mipLevelCount = 1,
                .sampleCount = 1
            }));

            wgpuQueueWriteTexture(
                webgpu.queue,
                to_ptr<WGPUTexelCopyTextureInfo>({ .texture = data }),
                image_data,
                width * height * 4,
                to_ptr<WGPUTexelCopyBufferLayout>({ .bytesPerRow = (uint32_t)(width * 4), .rowsPerImage = (uint32_t)height }),
                to_ptr(WGPUExtent3D{ (uint32_t)width, (uint32_t)height, 1 })
            );

            texture_view = wgpuTextureCreateView(data, nullptr);
            bind_group_2d = wgpuDeviceCreateBindGroup(webgpu.device, to_ptr(WGPUBindGroupDescriptor{
                .label = WGPU_STRING_VIEW("Texture Bind Group 2D"),
                .layout = webgpu.layout_2d,
                .entryCount = 3,
                // The entries `.binding` matches what we wrote in the shader.
                .entries = to_ptr<WGPUBindGroupEntry>({
                    {
                        .binding = 0,
                        .buffer = webgpu.uniforms_buffer_2d,
                        .size = sizeof(Uniforms2D)
                    },
                    {
                        .binding = 1,
                        .sampler = webgpu.sampler,
                    },
                    {
                        .binding = 2,
                        .textureView = texture_view
                    }
                })
            }));

            bind_group_3d = wgpuDeviceCreateBindGroup(webgpu.device, to_ptr(WGPUBindGroupDescriptor{
                .label = WGPU_STRING_VIEW("Texture Bind Group 3D"),
                .layout = webgpu.layout_3d,
                .entryCount = 4,
                // The entries `.binding` matches what we wrote in the shader.
                .entries = to_ptr<WGPUBindGroupEntry>({
                    {
                        .binding = 0,
                        .buffer = webgpu.uniforms_buffer_3d,
                        .size = sizeof(Uniforms3D)
                    },
                    {
                        .binding = 1,
                        .sampler = webgpu.sampler,
                    },
                    {
                        .binding = 2,
                        .textureView = texture_view
                    },

                    {
                        .binding = 3,
                        .buffer = webgpu.light_buffer,
                        .size = sizeof(LightData) * NUM_LIGHTS
                    },
                })
            }));
        }

        Texture(Texture&) = delete;
        Texture& operator=(Texture&) = delete;

        Texture(Texture&& other) noexcept {
            if (this != &other) {
                data = other.data;
                bind_group_2d = other.bind_group_2d;
                bind_group_3d = other.bind_group_3d;
                texture_view = other.texture_view;

                width = other.width;
                height = other.height;

                other.invalidate();
            }
        }

        Texture& operator=(Texture&& other) noexcept {
            if (this != &other) {
                destroy();

                data = other.data;
                bind_group_2d = other.bind_group_2d;
                bind_group_3d = other.bind_group_3d;
                texture_view = other.texture_view;

                width = other.width;
                height = other.height;

                other.invalidate();
            }

            return *this;
        }

        ~Texture() {
            destroy();
        };
    };

    struct TextureLoader : public ILoadResources {
        std::shared_ptr<GraphicsManager::WebGPUState> webgpu;

        TextureLoader(std::shared_ptr<GraphicsManager::WebGPUState> webgpu) : webgpu(webgpu) {}

        std::optional<Resource> load_resource(const std::filesystem::path&, std::ifstream& file_stream, std::string_view resource_path) override {
            auto file_data = get_data_from_file_stream(file_stream);
            int width, height, channels;
            unsigned char* image_data = stbi_load_from_memory(
                file_data.data(),
                (int)(file_data.size()),
                &width, &height, &channels, 4
            );

            if (image_data == nullptr) {
                SPDLOG_TRACE("Failed to load image file {}: {}", resource_path, stbi_failure_reason());
                return {};
            }

            SPDLOG_TRACE("Loaded image file {}. Texture size: {}x{}", resource_path, width, height);

            Resource resource = Texture(*webgpu, image_data, width, height, resource_path);

            stbi_image_free(image_data);

            return resource;
        }
    };

    struct glTFScene {
        struct MeshBundle {
            WGPUBuffer buffer;
            size_t vertex_count;
            std::string diffuse_texture;
            std::string mesh_name;

            bool should_draw = true;
            AABB aabb;

            MeshBundle(WGPUBuffer buffer, size_t vertex_count, std::string diffuse_texture, std::string mesh_name, AABB aabb) :
                buffer(buffer), vertex_count(vertex_count), diffuse_texture(diffuse_texture), mesh_name(mesh_name), aabb(aabb)
            {}
        };

        struct SceneObject {
            mat4 transform;
            u32 mesh_index;
            std::string name;
            AABB aabb;
        };

        std::vector<MeshBundle> mesh_bundles;
        std::vector<SceneObject> objects;

        void process_node(const aiScene* scene, const aiNode* node) {
            SPDLOG_TRACE("found gltf scene object {}", node->mName.C_Str());
            if (node->mNumMeshes != 0) {
                if (node->mNumMeshes > 1) {
                    SPDLOG_ERROR("scene object {} has more than one mesh. it will not be imported correctly.");
                }
                mat4 matrix = to_glm(node->mTransformation);

                AABB aabb = mesh_bundles[*node->mMeshes].aabb;
                aabb.center = matrix * vec4(aabb.center, 1);
                aabb.half_size = matrix * vec4(aabb.half_size, 0);

                objects.push_back(SceneObject {
                    .transform = matrix,
                    .mesh_index = *node->mMeshes,
                    .name = node->mName.C_Str(),
                    .aabb = aabb,
                });
            } 

            for (u32 idx = 0; idx < node->mNumChildren; idx++) {
                process_node(scene, node->mChildren[idx]);
            }
        }

        glTFScene(GraphicsManager::WebGPUState& webgpu, const aiScene* scene, std::string_view resource_path, Engine& engine) {
            // go through every mesh and convert it into usable vertex data on the GPU
            for (u32 idx = 0; idx < scene->mNumMeshes; idx++) {
                auto* mesh = scene->mMeshes[idx];
                size_t vertex_data_size = mesh->mNumFaces * 3 * sizeof(VertexData3D);
                VertexData3D* vertex_data = static_cast<VertexData3D*>(malloc(vertex_data_size));

                for (u32 face = 0; face < mesh->mNumFaces; face++) {
                    assert(mesh->mNormals);
                    for (u32 idx = 0; idx < 3; idx++) {
                        u32 vertex_id = mesh->mFaces[face].mIndices[idx];

                        u32 index = (face * 3) + idx;
                        vertex_data[index].x = mesh->mVertices[vertex_id].x;
                        vertex_data[index].y = mesh->mVertices[vertex_id].y;
                        vertex_data[index].z = mesh->mVertices[vertex_id].z;

                        vertex_data[index].nx = mesh->mNormals[vertex_id].x;
                        vertex_data[index].ny = mesh->mNormals[vertex_id].y;
                        vertex_data[index].nz = mesh->mNormals[vertex_id].z;

                        
                        if (mesh->mTextureCoords[0]) {
                            vertex_data[index].u = mesh->mTextureCoords[0][vertex_id].x;
                            vertex_data[index].v = mesh->mTextureCoords[0][vertex_id].y;
                        } else {
                            vertex_data[index].u = 0;
                            vertex_data[index].v = 0;
                        }
                    }
                }

                WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(webgpu.device, to_ptr(WGPUBufferDescriptor {
                    .label = WGPUStringView(resource_path.data(), resource_path.size()),
                    .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                    .size = vertex_data_size,
                }));
                wgpuQueueWriteBuffer(webgpu.queue, vertex_buffer, 0, vertex_data, vertex_data_size);

                std::string texture_resource_path = BLANK_TEXTURE_PATH.data();
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
                aiString texturePath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                    if (*texturePath.C_Str() == '*') {
                        u32 texture_index = std::stoi(texturePath.C_Str() + 1);
                        aiTexture* texture = scene->mTextures[texture_index];

                        if (texture->mHeight != 0) {
                            SPDLOG_CRITICAL("TODO: uncompressed textures");
                            std::abort();
                        }

                        
                        std::string texture_resource_name = std::format("::{}::{}", resource_path, texture_index);
                        if (!engine.resources->has_resource<Texture>(texture_resource_name)) {
                            int width, height, channels;
                            u8* image_data = stbi_load_from_memory((u8*)texture->pcData, texture->mWidth, &width, &height, &channels, 4);

                            SPDLOG_TRACE("found texture {} of size {}x{}", texture_index, width, height);

                            Texture texture_resource(webgpu, image_data, width, height, texture_resource_name);
                            engine.resources->insert_resource(texture_resource_name, std::move(texture_resource));
                            stbi_image_free(image_data);
                        }
                        texture_resource_path = texture_resource_name;

                        
                    } else {
                        SPDLOG_CRITICAL("TODO: external textures");
                        std::abort();
                    }
                } else {
                    // TODO: untextured meshes
                }

                // SPDLOG_TRACE("{}: ({}, {}, {})   ({}, {}, {})", 
                //         mesh->mName.C_Str(), 
                //         mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z, 
                //         mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z 
                // );

                mesh_bundles.push_back(MeshBundle(
                    vertex_buffer, 
                    mesh->mNumFaces * 3, 
                    texture_resource_path,
                    mesh->mName.C_Str(),
                    mesh->mAABB
                ));

                SPDLOG_TRACE("loaded {} vertices", mesh->mNumVertices);

                free(vertex_data);
            }

            // go through every object in the scene and add it to the object list
            process_node(scene, scene->mRootNode);
            SPDLOG_TRACE("objects acquired: {}", objects.size());
        }

        glTFScene(glTFScene&) = delete;
        glTFScene& operator=(glTFScene&) = delete;
        
        glTFScene(glTFScene&& other) = default;
        glTFScene& operator=(glTFScene&& other) = default;
        
        ~glTFScene() {
            for (auto bundle : mesh_bundles) {
                wgpuBufferRelease(bundle.buffer);
            }
        }
    };

    struct glTFSceneLoader : public ILoadResources {
        std::shared_ptr<GraphicsManager::WebGPUState> webgpu;
        Assimp::Importer importer;
        Engine& engine;
        glTFSceneLoader(std::shared_ptr<GraphicsManager::WebGPUState> webgpu, Engine& engine) : webgpu(webgpu), engine(engine) {}

        std::optional<Resource> load_resource(const std::filesystem::path& file_path, std::ifstream& file_stream, std::string_view resource_path) override {
            if (!resource_path.ends_with(".gltf") && !resource_path.ends_with(".glb")) {
                return {};
            }

            const aiScene* scene = importer.ReadFile(
                (char*)file_path.u8string().c_str(),
                aiProcess_Triangulate |
                aiProcess_FlipUVs     |
                aiProcess_GenBoundingBoxes 
            );

            if (nullptr == scene) {
                SPDLOG_TRACE("failed to load mesh: {}. reason: {}", resource_path, importer.GetErrorString());
                return {};
            }

            return glTFScene(*webgpu, scene, resource_path, engine);
        }
    };

    struct FontData {
        static constexpr u8 FIRST_CHAR = 32;
        static constexpr u8 NUM_CHARS = 95;

        stbtt_fontinfo font;
        float scale;
        struct {
            float u0, v0, u1, v1;  // UV coordinates
            int width, height;
        } char_uvs[NUM_CHARS];

        std::unique_ptr<Texture> texture;

        FontData(GraphicsManager::WebGPUState& webgpu, const u8* font_data) {
            // load font file
            stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));
            scale = stbtt_ScaleForPixelHeight(&font, 32);

            // setup texture pack
            stbrp_rect rects[NUM_CHARS];
            u8* char_bitmaps[NUM_CHARS];

            for (int i = 0; i < NUM_CHARS; i++) {
                int codepoint = FIRST_CHAR + i;
                int w, h, xoff, yoff;
                
                // Rasterize character
                char_bitmaps[i] = stbtt_GetCodepointBitmap(&font, 0, scale, 
                    codepoint, &w, &h, &xoff, &yoff);
                
                // Set up rectangle for packing
                rects[i].w = w;
                rects[i].h = h;
                rects[i].id = i;
            }

            // pack glyphs into rectangle
            stbrp_context context;
            int texture_width = 512, texture_height = 512;
            stbrp_node* nodes = static_cast<stbrp_node*>(malloc(texture_width * sizeof(stbrp_node)));
            stbrp_init_target(&context, texture_width, texture_height, nodes, texture_width);
            stbrp_pack_rects(&context, rects, NUM_CHARS);

            // generate texture
            u8* atlas = static_cast<u8*>(calloc(texture_width * texture_height * 4, 1));

            for (int i = 0; i < NUM_CHARS; i++) {
                if (rects[i].was_packed) {
                    // Copy bitmap into atlas at (rects[i].x, rects[i].y)
                    for (int y = 0; y < rects[i].h; y++) {
                        for (int x = 0; x < rects[i].w; x++) {
                            int src_idx = y * rects[i].w + x;
                            int dst_idx = ((rects[i].y + y) * texture_width + (rects[i].x + x)) * 4;

                            unsigned char alpha = char_bitmaps[i][src_idx];
                            atlas[dst_idx + 0] = 255;  // R
                            atlas[dst_idx + 1] = 255;  // G
                            atlas[dst_idx + 2] = 255;  // B
                            atlas[dst_idx + 3] = alpha; // A
                        }
                    }
                    
                    // Store UV coordinates (normalized 0-1)
                    char_uvs[i].u0 = (float)rects[i].x / texture_width;
                    char_uvs[i].v0 = (float)rects[i].y / texture_height;
                    char_uvs[i].u1 = (float)(rects[i].x + rects[i].w) / texture_width;
                    char_uvs[i].v1 = (float)(rects[i].y + rects[i].h) / texture_height;
                    char_uvs[i].width = rects[i].w;
                    char_uvs[i].height = rects[i].h;
                } else {
                    SPDLOG_ERROR("rects[i].was_packed == false");
                    std::abort();
                }
                free(char_bitmaps[i]);
            }

            // upload it to the gpu
            texture = std::make_unique<Texture>(webgpu, atlas, texture_width, texture_height, "Font texture");

            free(atlas);
            free(nodes);
        }
    };

    void write_to_light_buffer(GraphicsManager::WebGPUState& webgpu, Engine& engine) {
        LightData lights[NUM_LIGHTS];
        memset(lights, 0, sizeof(lights));
        u32 idx = 0;
        for (auto [transform, light] : engine.ecs->query<GlobalTransform, Light>()) {
            if (idx == 8) {
                SPDLOG_ERROR("more than 8 lights in the scene! 8 is the max number of lights");
                break;
            }

            lights[idx] = LightData {
                .ambient = vec4(light->ambient, 1.),
                .diffuse = vec4(light->diffuse, 1.),
                .specular = vec4(light->specular, 1.),
                .position = transform->model * vec4(0.f, 0.f, 0.f, 1.f),
                .distance = light->distance
            };
            idx++;
        }
        wgpuQueueWriteBuffer(webgpu.queue, webgpu.light_buffer, 0, &lights, sizeof(lights));
    }
}

void GraphicsManager::WebGPUState::configure_surface(GLFWwindow* window) {
    // recreate the frame buffer on resize
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    wgpuSurfaceConfigure(surface, to_ptr(WGPUSurfaceConfiguration{
        .device = device,
        .format = wgpuSurfaceGetPreferredFormat(surface, adapter),
        .usage = WGPUTextureUsage_RenderAttachment,
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .presentMode = WGPUPresentMode_Fifo // Explicitly set this because of a Dawn bug
    }));

    // recreate the depth texture on resize
    if (depth_texture != nullptr) {
        wgpuTextureRelease(depth_texture);
    }

    depth_texture = wgpuDeviceCreateTexture(device, to_ptr(WGPUTextureDescriptor {
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size =  { .width = (u32)width, .height = (u32)height, .depthOrArrayLayers = 1 },
        .format = WGPUTextureFormat_Depth24Plus,
        .mipLevelCount = 1,
        .sampleCount = 1,
    }));
}

static std::unique_ptr<FontData> da_font_data;
void GraphicsManager::WebGPUState::setup(GLFWwindow* window) {
    instance = wgpuCreateInstance(to_ptr(WGPUInstanceDescriptor{}));

    surface = glfwCreateWindowWGPUSurface(instance, window);

    adapter = nullptr;
    wgpuInstanceRequestAdapter(
        instance,
        to_ptr( WGPURequestAdapterOptions{ .featureLevel = WGPUFeatureLevel_Core, .compatibleSurface = surface } ),
        WGPURequestAdapterCallbackInfo{
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = []( WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* adapter_ptr, void* ) {
                if( status != WGPURequestAdapterStatus_Success ) {
                    SPDLOG_ERROR("Failed to get a WebGPU adapter: {}", std::string_view(message.data, message.length));
                    glfwTerminate();
                }

                *static_cast<WGPUAdapter*>(adapter_ptr) = adapter;
            },
            .userdata1 = &(adapter)
        }
    );
    while(!adapter) wgpuInstanceProcessEvents(instance);
    assert(adapter);

    device = nullptr;
    wgpuAdapterRequestDevice(
        adapter,
        to_ptr(WGPUDeviceDescriptor{
            // Add an error callback for more debug info
            .uncapturedErrorCallbackInfo = { .callback = []( WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void* ) {
                SPDLOG_ERROR("WebGPU uncaptured error type {} with message: {}", int(type), std::string_view(message.data, message.length));
            }}
        }),
        WGPURequestDeviceCallbackInfo{
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* device_ptr, void*) {
                if(status != WGPURequestDeviceStatus_Success) {
                    SPDLOG_ERROR("Failed to get a WebGPU device: {}", std::string_view(message.data, message.length));
                    glfwTerminate();
                }
                
                *static_cast<WGPUDevice*>(device_ptr) = device;
            },
            .userdata1 = &(device)
        }
    );
    while(!device) wgpuInstanceProcessEvents(instance);
    assert(device);

    queue = wgpuDeviceGetQueue(device);

    // setup our framebuffer
    configure_surface(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int, int) {
        ((Engine*)glfwGetWindowUserPointer(window))->gfx->webgpu->configure_surface(window);
    });

}

void GraphicsManager::WebGPUState::init_pipeline_2d() {
    // A vertex buffer containing a textured square.
    const VertexData2D vertices[] = {
          // position       // texcoords
        { -.5f,  -.5f,    0.0f,  1.0f },
        {  .5f,  -.5f,    1.0f,  1.0f },
        { -.5f,   .5f,    0.0f,  0.0f },

        {  .5f,  -.5f,    1.0f,  1.0f },
        { -.5f,   .5f,    0.0f,  0.0f },
        {  .5f,   .5f,    1.0f,  0.0f },
    };

    // give the GPU our quad
    quad_vertex_buffer = wgpuDeviceCreateBuffer( device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView( "2D Vertex Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(vertices)
    }) );
    wgpuQueueWriteBuffer(queue, quad_vertex_buffer, 0, vertices, sizeof(vertices));

    // uniforms
    uniforms_buffer_2d = wgpuDeviceCreateBuffer( device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView( "Uniform Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = sizeof(Uniforms2D)
    }) );

    // texture sampler
    sampler = wgpuDeviceCreateSampler( device, to_ptr( WGPUSamplerDescriptor{
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .maxAnisotropy = 1
    }));

    WGPUShaderSourceWGSL source_desc = {};
    source_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    source_desc.code = WGPUStringView { (char*)gpipeline_2d_shaderData, WGPU_STRLEN };
    // Point to the code descriptor from the shader descriptor.
    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &source_desc.chain;
    shader_module_2d = wgpuDeviceCreateShaderModule( device, &shader_desc );   

    pipeline_2d = wgpuDeviceCreateRenderPipeline( device, to_ptr( WGPURenderPipelineDescriptor{
        // Describe the vertex shader inputs
        .vertex = {
            .module = shader_module_2d,
            .entryPoint = WGPUStringView{ "vertex_shader_main", std::string_view("vertex_shader_main").length() },
            // Vertex attributes.
            .bufferCount = 2,
            .buffers = to_ptr<WGPUVertexBufferLayout>({
                // We have one buffer with our per-vertex position and UV data. This data never changes.
                // Note how the type, byte offset, and stride (bytes between elements) exactly matches our `vertex_buffer`.
                {
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .arrayStride = 4*sizeof(float),
                    .attributeCount = 2,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        // Position x,y are first.
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = 0,
                            .shaderLocation = 0
                        },
                        // Texture coordinates u,v are second.
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = 2*sizeof(float),
                            .shaderLocation = 1
                        }
                        })
                },
                // We will use a second buffer with our per-sprite translation and scale. This data will be set in our draw function.
                {
                    // This data is per-instance. All four vertices will get the same value. Each instance of drawing the vertices will get a different value.
                    // The type, byte offset, and stride (bytes between elements) exactly match the array of `InstanceData` structs we will upload in our draw function.
                    .stepMode = WGPUVertexStepMode_Instance,
                    .arrayStride = sizeof(InstanceData2D),
                    .attributeCount = 2,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        // Translation as a 3D vector.
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(InstanceData2D, translation),
                            .shaderLocation = 2
                        },
                        // Scale as a 2D vector for non-uniform scaling.
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = offsetof(InstanceData2D, scale),
                            .shaderLocation = 3
                        }
                        })
                }
                })
            },
        
        .primitive = WGPUPrimitiveState{
            .topology = WGPUPrimitiveTopology_TriangleList,
            },

        .depthStencil = to_ptr(WGPUDepthStencilState {
            .format = WGPUTextureFormat_Depth24Plus,
            .depthWriteEnabled = WGPUOptionalBool_True,
            .depthCompare = WGPUCompareFunction_Less,
        }),
        
        // No multi-sampling (1 sample per pixel, all bits on).
        .multisample = WGPUMultisampleState{
            .count = 1,
            .mask = ~0u
            },
        
        // Describe the fragment shader and its output
        .fragment = to_ptr( WGPUFragmentState{
            .module = shader_module_2d,
            .entryPoint = WGPUStringView{ "fragment_shader_main", std::string_view("fragment_shader_main").length() },
            
            // Our fragment shader outputs a single color value per pixel.
            .targetCount = 1,
            .targets = to_ptr<WGPUColorTargetState>({
                {
                    .format = wgpuSurfaceGetPreferredFormat( surface, adapter ),
                    // The images we want to draw may have transparency, so let's turn on alpha blending with over compositing (ɑ⋅foreground + (1-ɑ)⋅background).
                    // This will blend with whatever has already been drawn.
                    .blend = to_ptr( WGPUBlendState{
                        // Over blending for color
                        .color = {
                            .operation = WGPUBlendOperation_Add,
                            .srcFactor = WGPUBlendFactor_SrcAlpha,
                            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha
                            },
                        // Leave destination alpha alone
                        .alpha = {
                            .operation = WGPUBlendOperation_Add,
                            .srcFactor = WGPUBlendFactor_Zero,
                            .dstFactor = WGPUBlendFactor_One
                            }
                        } ),
                    .writeMask = WGPUColorWriteMask_All
                }})
            } )
    } ) );

    layout_2d = wgpuRenderPipelineGetBindGroupLayout(pipeline_2d, 0);
}

VertexData3D cube_vertices[] = {
    // +Z face
    { -1,  1,  1,    0, 0,    0, 0, 1 }, // top left
    { -1, -1,  1,    0, 1,    0, 0, 1 }, // bottom left
    {  1,  1,  1,    1, 0,    0, 0, 1 }, // top right

    {  1,  1,  1,    1, 0,    0, 0, 1 }, // top right
    { -1, -1,  1,    0, 1,    0, 0, 1 }, // bottom left
    {  1, -1,  1,    1, 1,    0, 0, 1 }, // bottom right


    // -Z face
    {  1,  1, -1,    1, 0,    0, 0, -1 }, // top right
    { -1, -1, -1,    0, 1,    0, 0, -1 }, // bottom left
    { -1,  1, -1,    0, 0,    0, 0, -1 }, // top left

    {  1, -1, -1,    1, 1,    0, 0, -1 }, // bottom right
    { -1, -1, -1,    0, 1,    0, 0, -1 }, // bottom left
    {  1,  1, -1,    1, 0,    0, 0, -1 }, // top right
    

    // +Y face
    { -1,  1, -1,    0, 0,    0, 1, 0 }, // top left
    { -1,  1,  1,    0, 1,    0, 1, 0 }, // bottom left
    {  1,  1, -1,    1, 0,    0, 1, 0 }, // top right

    {  1,  1, -1,    1, 0,    0, 1, 0 }, // top right
    { -1,  1,  1,    0, 1,    0, 1, 0 }, // bottom left
    {  1,  1,  1,    1, 1,    0, 1, 0 }, // bottom right
    

    // -Y face
    {  1, -1, -1,    1, 0,    0, -1, 0 }, // top right
    { -1, -1,  1,    0, 1,    0, -1, 0 }, // bottom left
    { -1, -1, -1,    0, 0,    0, -1, 0 }, // top left

    {  1, -1,  1,    1, 1,    0, -1, 0 }, // bottom right
    { -1, -1,  1,    0, 1,    0, -1, 0 }, // bottom left
    {  1, -1, -1,    1, 0,    0, -1, 0 }, // top right


    // +X face
    {  1,  1,  1,    0, 0,    1, 0, 0 }, // top left
    {  1, -1,  1,    0, 1,    1, 0, 0 }, // bottom left
    {  1,  1, -1,    1, 0,    1, 0, 0 }, // top right

    {  1,  1, -1,    1, 0,    1, 0, 0 }, // top right
    {  1, -1,  1,    0, 1,    1, 0, 0 }, // bottom left
    {  1, -1, -1,    1, 1,    1, 0, 0 }, // bottom right


    // -X face
    { -1,  1, -1,    1, 0,    -1, 0, 0 }, // top right
    { -1, -1,  1,    0, 1,    -1, 0, 0 }, // bottom left
    { -1,  1,  1,    0, 0,    -1, 0, 0 }, // top left

    { -1, -1, -1,    1, 1,    -1, 0, 0 }, // bottom right
    { -1, -1,  1,    0, 1,    -1, 0, 0 }, // bottom left
    { -1,  1, -1,    1, 0,    -1, 0, 0 }, // top right
};

void GraphicsManager::WebGPUState::init_pipeline_3d() {
    cube_vertex_buffer = wgpuDeviceCreateBuffer(device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "3D Vertex Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(cube_vertices)
    }));
    wgpuQueueWriteBuffer(queue, cube_vertex_buffer, 0, cube_vertices, sizeof(cube_vertices));

    uniforms_buffer_3d = wgpuDeviceCreateBuffer(device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView("3D Uniform Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = sizeof(Uniforms3D)
    }));

    light_buffer = wgpuDeviceCreateBuffer( device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView("Light Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = sizeof(LightData) * NUM_LIGHTS
    }));

    WGPUShaderSourceWGSL source_desc = {};
    source_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    source_desc.code = WGPUStringView( (const char*)gpipeline_3d_shaderData, WGPU_STRLEN );
    // Point to the code descriptor from the shader descriptor.
    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &source_desc.chain;
    shader_module_3d = wgpuDeviceCreateShaderModule( device, &shader_desc );   

    WGPUShaderSourceWGSL collider_source_desc = {};
    collider_source_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    collider_source_desc.code = WGPUStringView( (const char*)gpipeline_colliders_shaderData, WGPU_STRLEN );
    // Point to the code descriptor from the shader descriptor.
    WGPUShaderModuleDescriptor collider_shader_desc = {};
    collider_shader_desc.nextInChain = &collider_source_desc.chain;
    shader_module_colliders = wgpuDeviceCreateShaderModule( device, &collider_shader_desc );   

    pipeline_3d = wgpuDeviceCreateRenderPipeline(device, to_ptr(WGPURenderPipelineDescriptor {
        .label = WGPUStringView { "3D pipeline", WGPU_STRLEN },
        .vertex = {
            .module = shader_module_3d,
            .entryPoint = WGPUStringView { "vertex", std::string_view("vertex").length() },
            .bufferCount = 1,
            .buffers = to_ptr<WGPUVertexBufferLayout>({
                {
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .arrayStride = sizeof(VertexData3D),
                    .attributeCount = 3,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = 0,
                            .shaderLocation = 0
                        },
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = offsetof(VertexData3D, u),
                            .shaderLocation = 1
                        },
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(VertexData3D, nx),
                            .shaderLocation = 2
                        },
                    }),
                },
            }),

        },

        .primitive = WGPUPrimitiveState { 
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_None,
            // .cullMode = WGPUCullMode_Back,
        },

        .depthStencil = to_ptr(WGPUDepthStencilState {
            .format = WGPUTextureFormat_Depth24Plus,
            .depthWriteEnabled = WGPUOptionalBool_True,
            .depthCompare = WGPUCompareFunction_Less,
        }),

        .multisample = WGPUMultisampleState {
            .count = 1,
            .mask = ~0u
        },

        .fragment = to_ptr(WGPUFragmentState {
            .module = shader_module_3d,
            .entryPoint = WGPUStringView{ "fragment", std::string_view("fragment").size() },
            .targetCount = 1,
            .targets = to_ptr<WGPUColorTargetState>({
                .format = wgpuSurfaceGetPreferredFormat(surface, adapter),
                .blend = to_ptr( WGPUBlendState{
                    .color = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_SrcAlpha,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha
                        },
                    .alpha = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_Zero,
                        .dstFactor = WGPUBlendFactor_One
                        }
                    } ),
                .writeMask = WGPUColorWriteMask_All
            })
        }),
    }));

    pipeline_colliders = wgpuDeviceCreateRenderPipeline(device, to_ptr(WGPURenderPipelineDescriptor {
        .label = WGPUStringView { "Collider pipeline", WGPU_STRLEN },
        .vertex = {
            .module = shader_module_colliders,
            .entryPoint = WGPUStringView { "vertex", std::string_view("vertex").length() },
            .bufferCount = 1,
            .buffers = to_ptr<WGPUVertexBufferLayout>({
                {
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .arrayStride = sizeof(VertexData3D),
                    .attributeCount = 3,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = 0,
                            .shaderLocation = 0
                        },
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = offsetof(VertexData3D, u),
                            .shaderLocation = 1
                        },
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(VertexData3D, nx),
                            .shaderLocation = 2
                        },
                    }),
                },
            }),

        },

        .primitive = WGPUPrimitiveState { 
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_None,
            // .cullMode = WGPUCullMode_Back,
        },

        .depthStencil = to_ptr(WGPUDepthStencilState {
            .format = WGPUTextureFormat_Depth24Plus,
            .depthWriteEnabled = WGPUOptionalBool_True,
            .depthCompare = WGPUCompareFunction_Less,
        }),

        .multisample = WGPUMultisampleState {
            .count = 1,
            .mask = ~0u
        },

        .fragment = to_ptr(WGPUFragmentState {
            .module = shader_module_colliders,
            .entryPoint = WGPUStringView{ "fragment", std::string_view("fragment").size() },
            .targetCount = 1,
            .targets = to_ptr<WGPUColorTargetState>({
                .format = wgpuSurfaceGetPreferredFormat(surface, adapter),
                .blend = to_ptr( WGPUBlendState{
                    .color = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_SrcAlpha,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha
                        },
                    .alpha = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_Zero,
                        .dstFactor = WGPUBlendFactor_One
                        }
                    } ),
                .writeMask = WGPUColorWriteMask_All
            })
        }),
    }));

    layout_3d = wgpuRenderPipelineGetBindGroupLayout(pipeline_3d, 0);
    layout_colliders = wgpuRenderPipelineGetBindGroupLayout(pipeline_colliders, 0);

    bind_group_colliders = wgpuDeviceCreateBindGroup(device, to_ptr(WGPUBindGroupDescriptor{
        .label = WGPU_STRING_VIEW("Texture Bind Group Colliders"),
        .layout = layout_colliders,
        .entryCount = 1,
        // The entries `.binding` matches what we wrote in the shader.
        .entries = to_ptr<WGPUBindGroupEntry>({
            {
                .binding = 0,
                .buffer = uniforms_buffer_3d,
                .size = sizeof(Uniforms3D)
            },
        })
    }));
}

GraphicsManager::WebGPUState::WebGPUState(GLFWwindow* window) {
    setup(window);
    init_pipeline_2d();
    init_pipeline_3d();

    // create font data
    da_font_data = std::make_unique<FontData>(*this, gliberation_sansData);
}

GraphicsManager::GraphicsManager(
        Engine& engine,
        const std::string_view& window_name,
        int window_width,
        int window_height
) : engine(engine) {
    if (!glfwInit()) {
        const char* error;
        glfwGetError(&error);
        SPDLOG_ERROR("Failed to initialize GLFW: {}", error);
        std::abort();
    }

    // We don't want GLFW to set up a graphics API.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Create the window.
    window = glfwCreateWindow(1280, 720, window_name.data(), nullptr, nullptr);
    if( !window )
    {
        const char* error;
        glfwGetError(&error);
        glfwTerminate();
        SPDLOG_ERROR("Failed to create GLFW window: {}", error);
        std::abort();
    }
    glfwSetWindowAspectRatio(window, window_width, window_height);
    glfwSetWindowUserPointer(window, &engine);

    webgpu = std::make_shared<WebGPUState>(window);
    engine.resources->register_resource_loader(std::make_unique<TextureLoader>(webgpu));
    engine.resources->register_resource_loader(std::make_unique<glTFSceneLoader>(webgpu, engine));

    #define BLACK 0, 0, 0, 255
    #define MAGENTA 255, 0, 255, 255
    const u8 missing_texture_data[] = {
        BLACK,   MAGENTA,
        MAGENTA, BLACK
    };
    const u8 blank_texture_data[] = { 255, 255, 255, 255 };
    
    engine.resources->insert_resource(MISSING_TEXTURE_PATH, Texture(*webgpu, missing_texture_data, 2, 2, MISSING_TEXTURE_PATH));
    engine.resources->insert_resource(BLANK_TEXTURE_PATH, Texture(*webgpu, blank_texture_data, 1, 1, BLANK_TEXTURE_PATH));

    engine.scripts->lua.new_usertype<glTFScene::SceneObject>(
        "SceneObject", sol::constructors<>(),
        "name", &glTFScene::SceneObject::name,
        "aabb", &glTFScene::SceneObject::aabb
    );
    engine.scripts->lua.new_usertype<glTFScene::MeshBundle>(
        "MeshBundle", sol::constructors<>(),
        "diffuse_texture", &glTFScene::MeshBundle::diffuse_texture,
        "should_draw", &glTFScene::MeshBundle::should_draw,
        "aabb", &glTFScene::MeshBundle::aabb,
        "name", &glTFScene::MeshBundle::mesh_name
    );
    engine.scripts->lua.new_usertype<glTFScene>(
        "glTFScene", sol::constructors<>(),
        "mesh_bundles", &glTFScene::mesh_bundles,
        "objects", &glTFScene::objects
    );

    sol::table gltf_namespace = engine.scripts->lua["Resources"];
    gltf_namespace.set_function("get_gltf", [&](std::string resource_path) {
            return engine.resources->get_resource<glTFScene>(resource_path).value();
    });
}

bool GraphicsManager::window_should_close() {
    return glfwWindowShouldClose(window);
}

void GraphicsManager::clear_screen(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0.05, 0.05, 0.075, 1. }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );

    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_2d );

    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandBufferRelease(command_buffer);
    wgpuCommandEncoderRelease(encoder);
}

static bool warn_flag_2d = false;
void GraphicsManager::draw_sprites(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    // == SETUP SPRITES
    auto it = engine.ecs->query<Transform, Sprite>();
    auto entities = std::vector(it.begin(), it.end());
    std::sort(entities.begin(), entities.end(), [](auto& l, auto& r) {
        return std::get<Transform*>(l)->position.z < std::get<Transform*>(r)->position.z;
    });

    if (entities.size() == 0) {
        if (!warn_flag_2d) SPDLOG_WARN("no sprites! returning early!");
        warn_flag_2d = true;
        return;
    }
    warn_flag_2d = false;

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);
    // clear screen and begin render pass
    // const f64 grey_intensity = 0.;
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0, 0, 0, 0 }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );

    WGPUBuffer instance_buffer = wgpuDeviceCreateBuffer( webgpu->device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView("Instance Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(InstanceData2D) * entities.size()
    }) );

    // ready the pipeline
    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_2d );
    wgpuRenderPassEncoderSetVertexBuffer( render_pass, 0 /* slot */, webgpu->quad_vertex_buffer, 0, 6 * sizeof(VertexData2D) );
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1 /* slot */, instance_buffer, 0, sizeof(InstanceData2D) * entities.size());

    // == SETUP & UPLOAD UNIFORMS
    // Start with an identity matrix.
    Uniforms2D uniforms{};
    uniforms.projection = mat4{1};
    // Scale x and y by 1/100.
    uniforms.projection[0][0] = uniforms.projection[1][1] = 1.f/100.f;
    // Scale the long edge by an additional 1/(long/short) = short/long.

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    // if( width < height ) {
    //     uniforms.projection[1][1] *= width;
    //     uniforms.projection[1][1] /= height;
    // } else {
    //     uniforms.projection[0][0] *= height;
    //     uniforms.projection[0][0] /= width;
    // }
    uniforms.projection = glm::ortho(-640.f, 640.f, -360.f, 360.f, -1.f, 1.f);
    wgpuQueueWriteBuffer( webgpu->queue, webgpu->uniforms_buffer_2d, 0, &uniforms, sizeof(Uniforms2D) );


    // draw the little guys
    int count = 0;
    for (auto [transform, sprite] : entities) {
        auto maybe_tex = engine.resources->get_resource<Texture>(sprite->resource_path);
        if (!maybe_tex.has_value()) {
            SPDLOG_ERROR("Texture {} not found!", sprite->resource_path);
            sprite->resource_path = MISSING_TEXTURE_PATH;
            maybe_tex = engine.resources->get_resource<Texture>(MISSING_TEXTURE_PATH);
        }

        Texture* tex = maybe_tex.value();
        vec2 scale;
        if( tex->width < tex->height ) {
            scale = vec2( f32(tex->width)/tex->height, 1.0 );
        } else {
            scale = vec2( 1.0, f32(tex->height)/tex->width );
        }

        InstanceData2D instance {
            .translation = transform->position,
            .scale = vec3(tex->width, tex->height, 1) * transform->scale
        };

        wgpuQueueWriteBuffer( webgpu->queue, instance_buffer, count * sizeof(InstanceData2D), &instance, sizeof(InstanceData2D) );

        wgpuRenderPassEncoderSetBindGroup( render_pass, 0, tex->bind_group_2d, 0, nullptr );
        wgpuRenderPassEncoderDraw( render_pass, 6, 1, 0, count );

        count++;
    }

    wgpuRenderPassEncoderEnd(render_pass);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(instance_buffer);
}

static bool warn_flag_text = false;
void GraphicsManager::draw_text(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    // == SETUP SPRITES
    auto it = engine.ecs->query<Transform, Text>();
    auto entities = std::vector(it.begin(), it.end());

    if (entities.size() == 0) {
        if (!warn_flag_text) SPDLOG_WARN("no text! returning early!");
        warn_flag_text = true;
        return;
    }
    warn_flag_text = false;

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    // clear screen and begin render pass
    // const f64 grey_intensity = 0.;
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0, 0, 0, 0 }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );

    WGPUBuffer instance_buffer = wgpuDeviceCreateBuffer( webgpu->device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView("Instance Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(InstanceData2D) * entities.size()
    }) );

    // ready the pipeline
    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_2d );
    // wgpuRenderPassEncoderSetVertexBuffer( render_pass, 0 /* slot */, webgpu->quad_vertex_buffer, 0, 4*4*sizeof(float) );
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1 /* slot */, instance_buffer, 0, sizeof(InstanceData2D) * entities.size());

    // == SETUP & UPLOAD UNIFORMS
    // Start with an identity matrix.
    Uniforms2D uniforms{};
    uniforms.projection = mat4{1};
    // Scale x and y by 1/100.
    uniforms.projection[0][0] = uniforms.projection[1][1] = 1.f/100.f;
    // Scale the long edge by an additional 1/(long/short) = short/long.

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    uniforms.projection = glm::ortho(-640.f, 640.f, -360.f, 360.f, -1.f, 1.f);
    wgpuQueueWriteBuffer( webgpu->queue, webgpu->uniforms_buffer_2d, 0, &uniforms, sizeof(Uniforms2D) );


    // draw the little guys
    int count = 0;
    for (auto [transform, text] : entities) {
        if (text->text.size() == 0) continue;

        u64 buffer_count = text->text.size() * 6;
        u64 buffer_size = sizeof(VertexData2D) * buffer_count;
        WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(webgpu->device, to_ptr(WGPUBufferDescriptor {
            .label = WGPUStringView( "Text Vertex Buffer", WGPU_STRLEN ),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = buffer_size
        }));

        VertexData2D* cpu_buf = Ocean::Allocator<VertexData2D>(engine.ecs->ocean).allocate(buffer_count);
        VertexData2D* write_head = cpu_buf;

        float x_advance = 0;
        for (const char c : text->text) {
            int char_idx = c - 32;
            // warn here?
            if (char_idx < 0 || char_idx > FontData::NUM_CHARS) continue;

            int advance_width;
            int lsb; // left side bearing
            stbtt_GetCodepointHMetrics(&da_font_data->font, c, &advance_width, &lsb);
            float x_offset = lsb * da_font_data->scale;

            float u0 = da_font_data->char_uvs[char_idx].u0;
            float u1 = da_font_data->char_uvs[char_idx].u1;
            float v0 = da_font_data->char_uvs[char_idx].v0;
            float v1 = da_font_data->char_uvs[char_idx].v1;

            float w = da_font_data->char_uvs[char_idx].width;
            float h = da_font_data->char_uvs[char_idx].height;

            *write_head = VertexData2D { x_offset + x_advance,     0, u0, v1 }; write_head++; // top-left
            *write_head = VertexData2D { x_offset + x_advance + w, 0, u1, v1 }; write_head++; // top-right
            *write_head = VertexData2D { x_offset + x_advance,     h, u0, v0 }; write_head++; // bottom-left

            *write_head = VertexData2D { x_offset + x_advance + w, 0, u1, v1 }; write_head++; // top-right
            *write_head = VertexData2D { x_offset + x_advance,     h, u0, v0 }; write_head++; // bottom-left
            *write_head = VertexData2D { x_offset + x_advance + w, h, u1, v0 }; write_head++; // bottom-right

            x_advance += advance_width * da_font_data->scale;
        }

        InstanceData2D instance {
            .translation = transform->position,
            .scale = transform->scale
        };

        wgpuQueueWriteBuffer(webgpu->queue, vertex_buffer, 0, cpu_buf, buffer_size);
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vertex_buffer, 0, buffer_size);
        wgpuQueueWriteBuffer( webgpu->queue, instance_buffer, count * sizeof(InstanceData2D), &instance, sizeof(InstanceData2D) );

        wgpuRenderPassEncoderSetBindGroup( render_pass, 0, da_font_data->texture->bind_group_2d, 0, nullptr );
        wgpuRenderPassEncoderDraw( render_pass, buffer_count, 1, 0, count );

        count++;
        wgpuBufferRelease(vertex_buffer);
    }

    wgpuRenderPassEncoderEnd(render_pass);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(instance_buffer);
}

static bool warn_flag_3d = false;
void GraphicsManager::draw_3d(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    auto it = engine.ecs->query<Entity, GLTF, GlobalTransform>() | 
        std::views::filter([&](auto t) { 
            return !std::get<GLTF*>(t)->resource_path.empty();
        }) |
        std::views::transform([&](auto t) { 
            auto& resource_path = std::get<GLTF*>(t)->resource_path;
            auto maybe_gltf_scene = engine.resources->get_resource<glTFScene>(resource_path); 

            std::optional<std::tuple<Entity, glTFScene*, GlobalTransform*>> ret;

            if (!maybe_gltf_scene.has_value()) {
                SPDLOG_ERROR("could not get glTF from {}.", resource_path);
                resource_path = "";

                return ret;
            }

            return std::optional(std::make_tuple(std::get<Entity>(t), maybe_gltf_scene.value(), std::get<GlobalTransform*>(t)));
        }) |
        std::views::filter([&](auto scene) { return scene.has_value(); }) |
        std::views::transform([&](auto scene) { return scene.value(); });

    u32 gltf_count = 0;
    for (auto _it = it.begin(); _it != it.end(); _it++) {
        auto scene = std::get<glTFScene*>(*_it);
        gltf_count += scene->objects.size();
        // for (auto& obj : std::get<glTFScene*>(*_it)->objects) {
        //     if (scene->mesh_bundles[obj.mesh_index].should_draw) gltf_count++;
        // }
    }
        
    if (gltf_count == 0) {
        if (!warn_flag_3d) {
            SPDLOG_TRACE("no GLTFs. skipping!");
        }
        warn_flag_3d = true;
        return;
    }

    warn_flag_3d = false;

    WGPUBuffer matrix_buffer = wgpuDeviceCreateBuffer(webgpu->device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "3D Matrix Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
        .size = 256 * gltf_count,
    }));

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    // clear screen and begin render pass
    // const f64 grey_intensity = 0.;
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0, 0, 0, 0 }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );


    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_3d );

    WGPUBindGroupLayout bind_group_layout_3d_2 = wgpuRenderPipelineGetBindGroupLayout(webgpu->pipeline_3d, 1);

    mat4 projection_matrix = glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 1000.f);

    GlobalTransform camera_transform = GlobalTransform({1}, {1});

    auto camera_it = engine.ecs->query<GlobalTransform, Camera>();
    if (!camera_it.empty()) camera_transform = *std::get<GlobalTransform*>(*camera_it.begin());
    vec3 camera_pos = camera_transform.model[3];

    mat4 view_matrix = glm::lookAt(
        camera_pos,
        vec3(camera_transform.model * vec4(vec3(0, 0, -1), 1)),
        camera_transform.normal * vec3(0, 1, 0)
    );

    Uniforms3D uniforms{};
    uniforms.view_projection = projection_matrix * view_matrix;
    uniforms.camera_pos = camera_pos;
    wgpuQueueWriteBuffer(webgpu->queue, webgpu->uniforms_buffer_3d, 0, &uniforms, sizeof(Uniforms3D));

    write_to_light_buffer(*webgpu, engine);

    u32 instance_counter = 0;
    for (auto [entity, scene, transform] : it) {
        Albedo default_albedo = vec4(1.);
        vec4 albedo = engine.ecs->get_native_component<Albedo>(entity).value_or(&default_albedo)->color;

        mat4 model_matrix = transform->model;
        mat4 normal_matrix = transform->normal;

        for (auto& obj : scene->objects) {
            InstanceData3D instance_data {
                model_matrix * obj.transform,
                normal_matrix[0],
                normal_matrix[1],
                normal_matrix[2],
                albedo
            };

            wgpuQueueWriteBuffer(webgpu->queue, matrix_buffer, instance_counter*256, &instance_data, sizeof(InstanceData3D));

            WGPUBindGroup matrix_bind_group = wgpuDeviceCreateBindGroup(webgpu->device, to_ptr(WGPUBindGroupDescriptor {
                .label = WGPUStringView("3D Matrix Bind Group", WGPU_STRLEN),
                .layout = bind_group_layout_3d_2,
                .entryCount = 1,
                .entries = to_ptr<WGPUBindGroupEntry>({
                    {
                        .binding = 0,
                        .buffer = matrix_buffer,
                        .offset = instance_counter * 256,
                        .size = sizeof(InstanceData3D),
                    },
                }),
            }));

            wgpuRenderPassEncoderSetBindGroup(render_pass, 1, matrix_bind_group, 0, nullptr);

            auto& bundle = scene->mesh_bundles[obj.mesh_index];
            if (!bundle.should_draw) continue;
            Texture* texture = engine.resources->get_resource<Texture>(bundle.diffuse_texture).value();
            wgpuRenderPassEncoderSetBindGroup(render_pass, 0, texture->bind_group_3d, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0 /* slot */, bundle.buffer, 0, bundle.vertex_count * sizeof(VertexData3D));
            wgpuRenderPassEncoderDraw(render_pass, bundle.vertex_count, 1, 0, 0);

            instance_counter++;
            wgpuBindGroupRelease(matrix_bind_group);
        }
    }

    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBindGroupLayoutRelease(bind_group_layout_3d_2);
    wgpuBufferRelease(matrix_buffer);
}

void GraphicsManager::draw_colliders(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    auto it = engine.ecs->query<Entity, GlobalTransform, Body>();

    u32 count = 0;
    for (auto _it = it.begin(); _it != it.end(); _it++) count++;

    WGPUBuffer matrix_buffer = wgpuDeviceCreateBuffer(webgpu->device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "3D Matrix Buffer (colliders)", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
        .size = 256 * count,
    }));

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    // clear screen and begin render pass
    // const f64 grey_intensity = 0.;
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0, 0, 0, 0 }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );

    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_colliders );

    WGPUBindGroupLayout bind_group_layout_3d_2 = wgpuRenderPipelineGetBindGroupLayout(webgpu->pipeline_colliders, 1);

    mat4 projection_matrix = glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 1000.f);

    GlobalTransform camera_transform = GlobalTransform({1}, {1});

    auto camera_it = engine.ecs->query<GlobalTransform, Camera>();
    if (!camera_it.empty()) camera_transform = *std::get<GlobalTransform*>(*camera_it.begin());

    mat4 view_matrix = glm::lookAt(
        vec3(camera_transform.model[3]),
        vec3(camera_transform.model * vec4(vec3(0, 0, -1), 1)),
        camera_transform.normal * vec3(0, 1, 0)
    );
    
    Uniforms3D uniforms{};
    uniforms.view_projection = projection_matrix * view_matrix;
    wgpuQueueWriteBuffer(webgpu->queue, webgpu->uniforms_buffer_3d, 0, &uniforms, sizeof(Uniforms3D));


    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, webgpu->bind_group_colliders, 0, nullptr);

    u32 instance_counter = 0;
    for (auto [entity, transform, body] : it) {
        mat4 model_matrix = transform->model;
        mat4 normal_matrix = transform->normal;

        InstanceData3D instance_data {
            glm::scale(glm::translate(model_matrix, body->collider.aabb.center), body->collider.aabb.half_size),
            normal_matrix[0],
            normal_matrix[1],
            normal_matrix[2],
            vec4(1.)
        };

        wgpuQueueWriteBuffer(webgpu->queue, matrix_buffer, instance_counter*256, &instance_data, sizeof(InstanceData3D));

        WGPUBindGroup matrix_bind_group = wgpuDeviceCreateBindGroup(webgpu->device, to_ptr(WGPUBindGroupDescriptor {
            .label = WGPUStringView("3D Matrix Bind Group", WGPU_STRLEN),
            .layout = bind_group_layout_3d_2,
            .entryCount = 1,
            .entries = to_ptr<WGPUBindGroupEntry>({
                {
                    .binding = 0,
                    .buffer = matrix_buffer,
                    .offset = instance_counter * 256,
                    .size = sizeof(InstanceData3D),
                },
            }),
        }));

        wgpuRenderPassEncoderSetBindGroup(render_pass, 1, matrix_bind_group, 0, nullptr);

        wgpuRenderPassEncoderSetVertexBuffer(
                render_pass, 
                0 /* slot */, 
                webgpu->cube_vertex_buffer, 
                0, 
                sizeof(cube_vertices)
        );

        wgpuRenderPassEncoderDraw(render_pass, sizeof(cube_vertices) / sizeof(cube_vertices[0]), 1, 0, 0);

        wgpuRenderPassEncoderSetBindGroup(render_pass, 1, matrix_bind_group, 0, nullptr);

        wgpuBindGroupRelease(matrix_bind_group);

        instance_counter++;
    }

    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBindGroupLayoutRelease(bind_group_layout_3d_2);
    wgpuBufferRelease(matrix_buffer);
}

static bool warn_flag_sprite_3d = false;
void GraphicsManager::draw_sprite_3d(WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    auto it = engine.ecs->query<Entity, Sprite3D, GlobalTransform>() |
        std::views::filter([&](auto t) {
            return !std::get<Sprite3D*>(t)->resource_path.empty();
        });

    if (it.begin() == it.end()) {
        if (!warn_flag_sprite_3d) {
            SPDLOG_TRACE("no sprite3d's, skipping");
        }
        warn_flag_sprite_3d = true;
        return;
    }
    warn_flag_sprite_3d = false;

    u32 sprite_count = 0;
    for (auto _it = it.begin(); _it != it.end(); _it++) sprite_count++;

    WGPUBuffer matrix_buffer = wgpuDeviceCreateBuffer(webgpu->device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "3D Matrix Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
        .size = 256 * sprite_count,
    }));
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = surface_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0, 0, 0, 0 }
        }}),
        .depthStencilAttachment = to_ptr(WGPURenderPassDepthStencilAttachment {
            .view = depth_texture_view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );

    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_3d );
    WGPUBindGroupLayout bind_group_layout_3d_2 = wgpuRenderPipelineGetBindGroupLayout(webgpu->pipeline_3d, 1);
    mat4 projection_matrix = glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 1000.f);

    GlobalTransform camera_transform = GlobalTransform({1}, {1});
    auto camera_it = engine.ecs->query<GlobalTransform, Camera>();
    if (!camera_it.empty()) camera_transform = *std::get<GlobalTransform*>(*camera_it.begin());
    vec3 camera_pos = camera_transform.model[3];
    mat4 view_matrix = glm::lookAt(
        camera_pos,
        vec3(camera_transform.model * vec4(vec3(0, 0, -1), 1)),
        camera_transform.normal * vec3(0, 1, 0)
    );

    Uniforms3D uniforms{};
    uniforms.view_projection = projection_matrix * view_matrix;
    uniforms.camera_pos = camera_pos;
    wgpuQueueWriteBuffer(webgpu->queue, webgpu->uniforms_buffer_3d, 0, &uniforms, sizeof(Uniforms3D));

    write_to_light_buffer(*webgpu, engine);

    VertexData3D vertices[] = {
        { -.5f, -.5f, 0.f, 0.f, 1.f, .0f, .0f, 1.f}, // top-left
        {  .5f, -.5f, 0.f, 1.f, 1.f, .0f, .0f, 1.f}, // top-right
        { -.5f,  .5f, 0.f, 0.f, 0.f, .0f, .0f, 1.f}, // bottom-left

        {  .5f, -.5f, 0.f, 1.f, 1.f, .0f, .0f, 1.f}, // top-right
        { -.5f,  .5f, 0.f, 0.f, 0.f, .0f, .0f, 1.f}, // bottom-left
        {  .5f,  .5f, 0.f, 1.f, 0.f, .0f, .0f, 1.f}, // bottom-right
    };
    WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(webgpu->device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "Sprite 3D Vertex Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = sizeof(vertices)
    }));
    wgpuQueueWriteBuffer(webgpu->queue, vertex_buffer, 0, vertices, sizeof(vertices));
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vertex_buffer, 0, sizeof(vertices));

    u32 instance_counter = 0;
    for (auto [entity, sprite, transform] : it) {
        auto maybe_tex = engine.resources->get_resource<Texture>(sprite->resource_path);
        if (!maybe_tex.has_value()) continue;
        Texture* texture = maybe_tex.value();

        Albedo default_albedo = vec4(1.);
        vec4 albedo = engine.ecs->get_native_component<Albedo>(entity).value_or(&default_albedo)->color;

        mat4 model_matrix = transform->model;
        mat4 normal_matrix = transform->normal;

        InstanceData3D instance_data {
            model_matrix,
            normal_matrix[0],
            normal_matrix[1],
            normal_matrix[2],
            albedo
        };

        wgpuQueueWriteBuffer(webgpu->queue, matrix_buffer, instance_counter*256, &instance_data, sizeof(InstanceData3D));

        WGPUBindGroup matrix_bind_group = wgpuDeviceCreateBindGroup(webgpu->device, to_ptr(WGPUBindGroupDescriptor {
            .label = WGPUStringView("3D Matrix Bind Group", WGPU_STRLEN),
            .layout = bind_group_layout_3d_2,
            .entryCount = 1,
            .entries = to_ptr<WGPUBindGroupEntry>({
                {
                    .binding = 0,
                    .buffer = matrix_buffer,
                    .offset = instance_counter * 256,
                    .size = sizeof(InstanceData3D),
                },
            }),
        }));

        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, texture->bind_group_3d, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 1, matrix_bind_group, 0, nullptr);
        wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

        instance_counter++;
        wgpuBindGroupRelease(matrix_bind_group);
    }


    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBindGroupLayoutRelease(bind_group_layout_3d_2);
    wgpuBufferRelease(matrix_buffer);

    wgpuBufferRelease(vertex_buffer);
}

void GraphicsManager::draw() {
    glfwPollEvents();

    WGPUSurfaceTexture surface_texture{};
    wgpuSurfaceGetCurrentTexture( webgpu->surface, &surface_texture );
    WGPUTextureView surface_texture_view = wgpuTextureCreateView( surface_texture.texture, nullptr );
    WGPUTextureView depth_texture_view = wgpuTextureCreateView(webgpu->depth_texture, nullptr);

    clear_screen(surface_texture_view, depth_texture_view);

    draw_3d(surface_texture_view, depth_texture_view);
    draw_sprite_3d(surface_texture_view, depth_texture_view);
    if (engine.render_collision_shapes)
        draw_colliders(surface_texture_view, depth_texture_view);

    draw_sprites(surface_texture_view, depth_texture_view);
    draw_text(surface_texture_view, depth_texture_view);

    wgpuSurfacePresent( webgpu->surface );
    wgpuTextureViewRelease(surface_texture_view);
    wgpuTextureViewRelease(depth_texture_view);
    wgpuTextureRelease(surface_texture.texture);
}

GraphicsManager::~GraphicsManager() {
    wgpuBufferRelease(webgpu->cube_vertex_buffer);
    wgpuBufferRelease(webgpu->uniforms_buffer_3d);
    wgpuBufferRelease(webgpu->light_buffer);
    wgpuShaderModuleRelease(webgpu->shader_module_3d);
    wgpuShaderModuleRelease(webgpu->shader_module_colliders);
    wgpuRenderPipelineRelease(webgpu->pipeline_colliders);
    wgpuRenderPipelineRelease(webgpu->pipeline_3d);
    wgpuTextureRelease(webgpu->depth_texture);
    wgpuBindGroupLayoutRelease(webgpu->layout_3d);

    wgpuBindGroupLayoutRelease(webgpu->layout_2d);
    wgpuRenderPipelineRelease(webgpu->pipeline_2d);
    wgpuShaderModuleRelease(webgpu->shader_module_2d);
    wgpuSamplerRelease(webgpu->sampler);
    wgpuBufferRelease(webgpu->uniforms_buffer_2d);
    wgpuBufferRelease(webgpu->quad_vertex_buffer);

    wgpuQueueRelease(webgpu->queue);
    wgpuDeviceRelease(webgpu->device);
    wgpuAdapterRelease(webgpu->adapter);
    wgpuSurfaceRelease(webgpu->surface);
    wgpuInstanceRelease(webgpu->instance);

    glfwDestroyWindow(window);
    glfwTerminate();
}
