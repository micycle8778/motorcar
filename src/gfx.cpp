#include <ranges>
#include <algorithm>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include "webgpu/webgpu.h"
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <incbin.h>

#include "gfx.h"
#include "resources.h"
#include "engine.h"
#include "types.h"
#include "components.h"

using namespace motorcar;

INCTXT(pipeline_2d_shader, "pipeline2d.wgsl");
INCTXT(pipeline_3d_shader, "pipeline3d.wgsl");

const std::string_view MISSING_TEXTURE_PATH = "::gfx::missing_texture";

struct GraphicsManager::WebGPUState {
    WGPUInstance instance = nullptr;
    WGPUSurface surface = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;

    WGPUBuffer cube_vertex_buffer = nullptr;
    WGPUBuffer uniforms_buffer_3d = nullptr;
    WGPUShaderModule shader_module_3d = nullptr;
    WGPURenderPipeline pipeline_3d = nullptr;
    WGPUTexture depth_texture = nullptr;
    WGPUBindGroupLayout layout_3d = nullptr;

    WGPUBuffer quad_vertex_buffer = nullptr;
    WGPUBuffer uniforms_buffer_2d = nullptr;
    WGPUSampler sampler = nullptr;
    WGPUShaderModule shader_module_2d = nullptr;
    WGPURenderPipeline pipeline_2d = nullptr;
    WGPUBindGroupLayout layout_2d = nullptr;

    WebGPUState(GLFWwindow*);

    void configure_surface(GLFWwindow* window);
    void setup(GLFWwindow*);
    void init_pipeline_2d();
    void init_pipeline_3d();
};

namespace {
    template< typename T > constexpr const T* to_ptr( const T& val ) { return &val; }
    template< typename T, std::size_t N > constexpr const T* to_ptr( const T (&&arr)[N] ) { return arr; }

    struct InstanceData {
        vec3 translation;
        vec2 scale;
        //float rotation;
    };

    struct Uniforms2D {
        mat4 projection;
    };

    struct Uniforms3D {
        mat4 view_projection;
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
        WGPUBindGroup bind_group = nullptr;
        WGPUTextureView texture_view = nullptr;
        size_t width = 0;
        size_t height = 0;

        void invalidate() {
            texture_view = nullptr;
            bind_group = nullptr;
            data = nullptr;
        }

        void destroy() {
            if (bind_group != nullptr) wgpuBindGroupRelease(bind_group);
            if (texture_view != nullptr) wgpuTextureViewRelease(texture_view);
            if (data != nullptr) wgpuTextureDestroy(data);

            invalidate();
        }

        Texture(GraphicsManager::WebGPUState& webgpu, const u8* image_data, size_t width, size_t height) {
            this->width = width;
            this->height = height;

            data = wgpuDeviceCreateTexture(webgpu.device, to_ptr(WGPUTextureDescriptor{
                //.label = WGPUStringView { .data = path.c_str(), .length = WGPU_STRLEN },
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
            bind_group = wgpuDeviceCreateBindGroup(webgpu.device, to_ptr(WGPUBindGroupDescriptor{
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
        }

        Texture(Texture&) = delete;
        Texture& operator=(Texture&) = delete;

        Texture(Texture&& other) noexcept {
            if (this != &other) {
                data = other.data;
                bind_group = other.bind_group;
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
                bind_group = other.bind_group;
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

            Resource resource = Texture(*webgpu, image_data, width, height);

            stbi_image_free(image_data);

            return resource;
        }
    };
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
    const struct {
        // position
        float x, y;
        // texcoords
        float u, v;
    } vertices[] = {
          // position       // texcoords
        { -1.0f,  -1.0f,    0.0f,  1.0f },
        {  1.0f,  -1.0f,    1.0f,  1.0f },
        { -1.0f,   1.0f,    0.0f,  0.0f },
        {  1.0f,   1.0f,    1.0f,  0.0f },
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
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .maxAnisotropy = 1
    }));

    WGPUShaderSourceWGSL source_desc = {};
    source_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    source_desc.code = WGPUStringView( gpipeline_2d_shaderData, std::string_view(gpipeline_2d_shaderData).length() );
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
                    .arrayStride = sizeof(InstanceData),
                    .attributeCount = 2,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        // Translation as a 3D vector.
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(InstanceData, translation),
                            .shaderLocation = 2
                        },
                        // Scale as a 2D vector for non-uniform scaling.
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = offsetof(InstanceData, scale),
                            .shaderLocation = 3
                        }
                        })
                }
                })
            },
        
        // Interpret our 4 vertices as a triangle strip
        .primitive = WGPUPrimitiveState{
            .topology = WGPUPrimitiveTopology_TriangleStrip,
            },
        
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

const struct {
    float x, y, z;
    float u, v;
} cube_vertices[] = {
    // +Z face
    { -1,  1,  1,    0, 0 }, // top left
    { -1, -1,  1,    0, 1 }, // bottom left
    {  1,  1,  1,    1, 0 }, // top right

    {  1,  1,  1,    1, 0 }, // top right
    { -1, -1,  1,    0, 1 }, // bottom left
    {  1, -1,  1,    1, 1 }, // bottom right


    // -Z face
    {  1,  1, -1,    1, 0 }, // top right
    { -1, -1, -1,    0, 1 }, // bottom left
    { -1,  1, -1,    0, 0 }, // top left

    {  1, -1, -1,    1, 1 }, // bottom right
    { -1, -1, -1,    0, 1 }, // bottom left
    {  1,  1, -1,    1, 0 }, // top right
    

    // +Y face
    { -1,  1, -1,    0, 0 }, // top left
    { -1,  1,  1,    0, 1 }, // bottom left
    {  1,  1, -1,    1, 0 }, // top right

    {  1,  1, -1,    1, 0 }, // top right
    { -1,  1,  1,    0, 1 }, // bottom left
    {  1,  1,  1,    1, 1 }, // bottom right
    

    // -Y face
    {  1, -1, -1,    1, 0 }, // top right
    { -1, -1,  1,    0, 1 }, // bottom left
    { -1, -1, -1,    0, 0 }, // top left

    {  1, -1,  1,    1, 1 }, // bottom right
    { -1, -1,  1,    0, 1 }, // bottom left
    {  1, -1, -1,    1, 0 }, // top right


    // +X face
    {  1,  1,  1,    0, 0 }, // top left
    {  1, -1,  1,    0, 1 }, // bottom left
    {  1,  1, -1,    1, 0 }, // top right

    {  1,  1, -1,    1, 0 }, // top right
    {  1, -1,  1,    0, 1 }, // bottom left
    {  1, -1, -1,    1, 1 }, // bottom right


    // -X face
    { -1,  1, -1,    1, 0 }, // top right
    { -1, -1,  1,    0, 1 }, // bottom left
    { -1,  1,  1,    0, 0 }, // top left

    { -1, -1, -1,    1, 1 }, // bottom right
    { -1, -1,  1,    0, 1 }, // bottom left
    { -1,  1, -1,    1, 0 }, // top right
};

void GraphicsManager::WebGPUState::init_pipeline_3d() {
    // lifted from webgpu samples

    cube_vertex_buffer = wgpuDeviceCreateBuffer(device, to_ptr(WGPUBufferDescriptor {
        .label = WGPUStringView( "2D Vertex Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(cube_vertices)
    }));
    wgpuQueueWriteBuffer(queue, cube_vertex_buffer, 0, cube_vertices, sizeof(cube_vertices));

    uniforms_buffer_3d = wgpuDeviceCreateBuffer( device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView( "Uniform Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = sizeof(Uniforms2D)
    }) );

    WGPUShaderSourceWGSL source_desc = {};
    source_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    source_desc.code = WGPUStringView( gpipeline_3d_shaderData, std::string_view(gpipeline_3d_shaderData).length() );
    // Point to the code descriptor from the shader descriptor.
    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &source_desc.chain;
    shader_module_3d = wgpuDeviceCreateShaderModule( device, &shader_desc );   

    pipeline_3d = wgpuDeviceCreateRenderPipeline(device, to_ptr(WGPURenderPipelineDescriptor {
        .vertex = {
            .module = shader_module_3d,
            .entryPoint = WGPUStringView { "vertex", std::string_view("vertex").length() },
            .bufferCount = 1,
            .buffers = to_ptr<WGPUVertexBufferLayout>({
                {
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .arrayStride = 5*sizeof(float), // xyz position, uv texcoords
                    .attributeCount = 2,
                    .attributes = to_ptr<WGPUVertexAttribute>({
                        {
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = 0,
                            .shaderLocation = 0
                        },
                        {
                            .format = WGPUVertexFormat_Float32x2,
                            .offset = 3*sizeof(float),
                            .shaderLocation = 1
                        },
                    }),
                }
            }),
        },

        .primitive = WGPUPrimitiveState { 
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_Back,
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

    layout_3d = wgpuRenderPipelineGetBindGroupLayout(pipeline_3d, 0);
}

GraphicsManager::WebGPUState::WebGPUState(GLFWwindow* window) {
    setup(window);
    init_pipeline_2d();
    init_pipeline_3d();
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
    window = glfwCreateWindow(window_width, window_height, window_name.data(), nullptr, nullptr);
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

    #define BLACK 0, 0, 0, 255
    #define MAGENTA 255, 0, 255, 255
    const u8 missing_texture_data[] = {
        BLACK,   MAGENTA,
        MAGENTA, BLACK
    };

    engine.resources->insert_resource(MISSING_TEXTURE_PATH, Texture(*webgpu, missing_texture_data, 2, 2));
}

bool GraphicsManager::window_should_close() {
    return glfwWindowShouldClose(window);
}

void GraphicsManager::clear_screen(WGPUTextureView surface_texture_view) {
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
        }})
    }) );

    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_2d );

    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );
}

static bool warn_flag = false;
void GraphicsManager::draw_sprites(WGPUTextureView surface_texture_view) {
    // == SETUP SPRITES
    auto it = engine.ecs->query<Transform, Sprite>();
    auto entities = std::vector(it.begin(), it.end());
    std::sort(entities.begin(), entities.end(), [](auto& l, auto& r) {
        return std::get<Transform*>(l)->position.z < std::get<Transform*>(r)->position.z;
    });

    if (entities.size() == 0) {
        if (!warn_flag) SPDLOG_WARN("no sprites! returning early!");
        warn_flag = true;
        return;
    }
    warn_flag = false;

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
        }})
    }) );

    WGPUBuffer instance_buffer = wgpuDeviceCreateBuffer( webgpu->device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView("Instance Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(InstanceData) * entities.size()
    }) );

    // ready the pipeline
    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_2d );
    wgpuRenderPassEncoderSetVertexBuffer( render_pass, 0 /* slot */, webgpu->quad_vertex_buffer, 0, 4*4*sizeof(float) );
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1 /* slot */, instance_buffer, 0, sizeof(InstanceData) * entities.size());

    // == SETUP & UPLOAD UNIFORMS
    // Start with an identity matrix.
    Uniforms2D uniforms{};
    uniforms.projection = mat4{1};
    // Scale x and y by 1/100.
    uniforms.projection[0][0] = uniforms.projection[1][1] = 1.f/100.f;
    // Scale the long edge by an additional 1/(long/short) = short/long.

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    if( width < height ) {
        uniforms.projection[1][1] *= width;
        uniforms.projection[1][1] /= height;
    } else {
        uniforms.projection[0][0] *= height;
        uniforms.projection[0][0] /= width;
    }
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

        InstanceData instance {
            .translation = transform->position,
            .scale = vec3(scale, 1) * transform->scale
        };

        wgpuQueueWriteBuffer( webgpu->queue, instance_buffer, count * sizeof(InstanceData), &instance, sizeof(InstanceData) );

        wgpuRenderPassEncoderSetBindGroup( render_pass, 0, tex->bind_group, 0, nullptr );
        wgpuRenderPassEncoderDraw( render_pass, 4, 1, 0, count );

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

void GraphicsManager::draw_3d(WGPUTextureView surface_texture_view) {
    // always draw 1 cube
    
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);
    WGPUTextureView depth_texture_view = wgpuTextureCreateView(webgpu->depth_texture, nullptr);

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
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.,
        }),
    }) );


    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline_3d );
    wgpuRenderPassEncoderSetVertexBuffer( render_pass, 0 /* slot */, webgpu->cube_vertex_buffer, 0, sizeof(cube_vertices) );

    mat4 projection_matrix = glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 100.f);
    mat4 view_matrix = glm::lookAt(
            vec3(0, 2, -3),
            vec3(0, 0, 0),
            vec3(0, 1, 0)
    );
    view_matrix = glm::rotate(view_matrix, (float)glfwGetTime(), vec3(0, 1, 0));

    Uniforms3D uniforms{};
    uniforms.view_projection = projection_matrix * view_matrix;
    wgpuQueueWriteBuffer(webgpu->queue, webgpu->uniforms_buffer_3d, 0, &uniforms, sizeof(Uniforms3D));

    WGPUBindGroup bind_group = wgpuDeviceCreateBindGroup(webgpu->device, to_ptr(WGPUBindGroupDescriptor {
        .layout = webgpu->layout_3d,
        .entryCount = 1,
        .entries = to_ptr<WGPUBindGroupEntry>({
            {
                .binding = 0,
                .buffer = webgpu->uniforms_buffer_3d,
                .size = sizeof(Uniforms3D)
            },
        }),
    }));

    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, bind_group, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, 36, 1, 0, 0);

    wgpuRenderPassEncoderEnd(render_pass);
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(encoder);
}

void GraphicsManager::draw() {
    glfwPollEvents();

    WGPUSurfaceTexture surface_texture{};
    wgpuSurfaceGetCurrentTexture( webgpu->surface, &surface_texture );
    WGPUTextureView surface_texture_view = wgpuTextureCreateView( surface_texture.texture, nullptr );

    clear_screen(surface_texture_view);
    draw_3d(surface_texture_view);
    draw_sprites(surface_texture_view);

    wgpuSurfacePresent( webgpu->surface );
    wgpuTextureViewRelease(surface_texture_view);
    wgpuTextureRelease(surface_texture.texture);
}

GraphicsManager::~GraphicsManager() {
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
