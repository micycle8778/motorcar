#include <ranges>
#include <algorithm>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include "webgpu/webgpu.h"
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "gfx.h"
#include "resources.h"
#include "engine.h"
#include "types.h"

using namespace motorcar;

const char* source = R"(
struct Uniforms {
    projection: mat4x4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var texData: texture_2d<f32>;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texcoords: vec2f,
    @location(2) translation: vec3f,
    @location(3) scale: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texcoords: vec2f,
};

@vertex
fn vertex_shader_main( in: VertexInput ) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.projection * vec4f( vec3f( in.scale * in.position, 0.0 ) + in.translation, 1.0 );
    out.texcoords = in.texcoords;
    return out;
}

@fragment
fn fragment_shader_main( in: VertexOutput ) -> @location(0) vec4f {
    var color = textureSample( texData, texSampler, in.texcoords ).rgba;
    return color;
}
)";

struct GraphicsManager::WebGPUState {
    WGPUInstance instance = nullptr;
    WGPUSurface surface = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;

    WGPUBuffer vertex_buffer = nullptr;
    WGPUBuffer uniforms_buffer = nullptr;
    WGPUSampler sampler = nullptr;
    WGPUShaderModule shader_module = nullptr;
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout layout = nullptr;

    WebGPUState(GLFWwindow*);

    void setup(GLFWwindow*);
    void init_pipeline(GLFWwindow*);
};

namespace {
    template< typename T > constexpr const T* to_ptr( const T& val ) { return &val; }
    template< typename T, std::size_t N > constexpr const T* to_ptr( const T (&&arr)[N] ) { return arr; }

    struct InstanceData {
        vec3 translation;
        vec2 scale;
        //float rotation;
    };

    struct Uniforms {
        mat4 projection;
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
        int width = 0;
        int height = 0;

        void invalidate() {
            texture_view = nullptr;
            bind_group = nullptr;
            data = nullptr;
        }

        void destroy() {
            if (bind_group != nullptr) wgpuBindGroupRelease(bind_group);
            if (texture_view != nullptr) wgpuTextureViewRelease(texture_view);
            if (data != nullptr) wgpuTextureDestroy(data);

            bind_group = nullptr;
            texture_view = nullptr;
            data = nullptr;
        }

        Texture(WGPUTexture data, WGPUBindGroup bind_group, WGPUTextureView texture_view, int width, int height) : 
            data(data), bind_group(bind_group), texture_view(texture_view), width(width), height(height) {}

        Texture(Texture&) = delete;
        Texture& operator=(Texture&) = delete;

        Texture(Texture&& other) {
            if (this != &other) {
                data = other.data;
                bind_group = other.bind_group;
                texture_view = other.texture_view;

                width = other.width;
                height = other.height;

                other.invalidate();
            }
        }

        Texture& operator=(Texture&& other) {
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

        std::optional<Resource> load_resource(const std::filesystem::path& path, std::ifstream& file_stream) override {
            auto file_data = get_data_from_file_stream(file_stream);
            int width, height, channels;
            unsigned char* image_data = stbi_load_from_memory(
                file_data.data(),
                (int)(file_data.size()),
                &width, &height, &channels, 4
            );

            spdlog::trace("{}, {}, {}, {}", image_data[0], image_data[1],image_data[2],image_data[3]);

            if (image_data == nullptr) {
                spdlog::trace("Failed to load image file {}: {}", path.string(), stbi_failure_reason());
                return {};
            }

            spdlog::trace("Loaded image file {}. Texture size: {}x{}", path.string(), width, height);

            WGPUTexture tex = wgpuDeviceCreateTexture(webgpu->device, to_ptr(WGPUTextureDescriptor{
                //.label = WGPUStringView { .data = path.c_str(), .length = WGPU_STRLEN },
                .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
                .dimension = WGPUTextureDimension_2D,
                .size = { (uint32_t)width, (uint32_t)height, 1 },
                .format = WGPUTextureFormat_RGBA8UnormSrgb,
                .mipLevelCount = 1,
                .sampleCount = 1
            }));

            wgpuQueueWriteTexture(
                webgpu->queue,
                to_ptr<WGPUTexelCopyTextureInfo>({ .texture = tex }),
                image_data,
                width * height * 4,
                to_ptr<WGPUTexelCopyBufferLayout>({ .bytesPerRow = (uint32_t)(width*4), .rowsPerImage = (uint32_t)height }),
                to_ptr( WGPUExtent3D{ (uint32_t)width, (uint32_t)height, 1 } )
            );

            WGPUTextureView texture_view = wgpuTextureCreateView( tex, nullptr );
            WGPUBindGroup bind_group = wgpuDeviceCreateBindGroup( webgpu->device, to_ptr( WGPUBindGroupDescriptor{
                .layout = webgpu->layout,
                .entryCount = 3,
                // The entries `.binding` matches what we wrote in the shader.
                .entries = to_ptr<WGPUBindGroupEntry>({
                    {
                        .binding = 0,
                        .buffer = webgpu->uniforms_buffer,
                        .size = sizeof( Uniforms )
                    },
                    {
                        .binding = 1,
                        .sampler = webgpu->sampler,
                    },
                    {
                        .binding = 2,
                        .textureView = texture_view
                    }
                    })
                } ) );

            stbi_image_free(image_data);

            return Resource(Texture(tex, bind_group, texture_view, width, height));
        }
    };
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
                    spdlog::error("Failed to get a WebGPU adapter: {}", std::string_view(message.data, message.length));
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
            .uncapturedErrorCallbackInfo = { .callback = []( WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void*, void* ) {
                spdlog::error("WebGPU uncaptured error type {} with message: {}", int(type), std::string_view(message.data, message.length));
            }}
        }),
        WGPURequestDeviceCallbackInfo{
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* device_ptr, void*) {
                if(status != WGPURequestDeviceStatus_Success) {
                    spdlog::error("Failed to get a WebGPU device: {}", std::string_view(message.data, message.length));
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
}

void GraphicsManager::WebGPUState::init_pipeline(GLFWwindow* window) {
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
    vertex_buffer = wgpuDeviceCreateBuffer( device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView( "Vertex Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(vertices)
    }) );
    wgpuQueueWriteBuffer(queue, vertex_buffer, 0, vertices, sizeof(vertices));

    // setup our framebuffer
    int width, height;
    glfwGetFramebufferSize( window, &width, &height );
    wgpuSurfaceConfigure( surface, to_ptr( WGPUSurfaceConfiguration{
        .device = device,
        .format = wgpuSurfaceGetPreferredFormat( surface, adapter ),
        .usage = WGPUTextureUsage_RenderAttachment,
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .presentMode = WGPUPresentMode_Fifo // Explicitly set this because of a Dawn bug
    }) );

    // uniforms
    uniforms_buffer = wgpuDeviceCreateBuffer( device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView( "Uniform Buffer", WGPU_STRLEN ),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = sizeof(Uniforms)
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
    source_desc.code = WGPUStringView( source, std::string_view(source).length() );
    // Point to the code descriptor from the shader descriptor.
    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &source_desc.chain;
    shader_module = wgpuDeviceCreateShaderModule( device, &shader_desc );   

    pipeline = wgpuDeviceCreateRenderPipeline( device, to_ptr( WGPURenderPipelineDescriptor{
        // Describe the vertex shader inputs
        .vertex = {
            .module = shader_module,
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
            .module = shader_module,
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

    layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
}

GraphicsManager::WebGPUState::WebGPUState(GLFWwindow* window) {
    setup(window);
    init_pipeline(window);
}

GraphicsManager::GraphicsManager(
        Engine& engine,
        const std::string& window_name,
        int window_width,
        int window_height
) : engine(engine) {
    if (!glfwInit()) {
        const char* error;
        glfwGetError(&error);
        spdlog::error("Failed to initialize GLFW: {}", error);
        std::abort();
    }

    // We don't want GLFW to set up a graphics API.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Create the window.
    window = glfwCreateWindow(window_width, window_height, window_name.c_str(), nullptr, nullptr);
    if( !window )
    {
        const char* error;
        glfwGetError(&error);
        glfwTerminate();
        spdlog::error("Failed to create GLFW window: {}", error);
        std::abort();
    }
    glfwSetWindowAspectRatio(window, window_width, window_height);
    glfwSetWindowUserPointer(window, &engine);
    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

    webgpu = std::make_shared<WebGPUState>(window);
    engine.resources->register_resource_loader(std::make_unique<TextureLoader>(webgpu));
}

bool GraphicsManager::window_should_close() {
    return glfwWindowShouldClose(window);
}

void GraphicsManager::draw(std::span<const Sprite> sprites) {
    glfwPollEvents();

    // == SETUP SPRITES
    std::vector<Sprite> sorted_sprites(sprites.begin(), sprites.end());
    std::sort(sorted_sprites.begin(), sorted_sprites.end(), [](Sprite s1, Sprite s2) { return s1.depth < s2.depth; });

    WGPUBuffer instance_buffer = wgpuDeviceCreateBuffer( webgpu->device, to_ptr( WGPUBufferDescriptor{
        .label = WGPUStringView("Instance Buffer", WGPU_STRLEN),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = sizeof(InstanceData) * sprites.size()
    }) );


    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(webgpu->device, nullptr);

    WGPUSurfaceTexture surface_texture{};
    wgpuSurfaceGetCurrentTexture( webgpu->surface, &surface_texture );
    WGPUTextureView current_texture_view = wgpuTextureCreateView( surface_texture.texture, nullptr );

    // clear screen and begin render pass
    // const f64 grey_intensity = 0.;
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass( encoder, to_ptr<WGPURenderPassDescriptor>({
        .colorAttachmentCount = 1,
        .colorAttachments = to_ptr<WGPURenderPassColorAttachment>({{
            .view = current_texture_view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // not a 3D texture
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            // Choose the background color.
            .clearValue = WGPUColor{ 0.05, 0.05, 0.075, 1. }
        }})
    }) );

    // ready the pipeline
    wgpuRenderPassEncoderSetPipeline( render_pass, webgpu->pipeline );
    wgpuRenderPassEncoderSetVertexBuffer( render_pass, 0 /* slot */, webgpu->vertex_buffer, 0, 4*4*sizeof(float) );
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1 /* slot */, instance_buffer, 0, sizeof(InstanceData) * sprites.size());

    // == SETUP & UPLOAD UNIFORMS
    // Start with an identity matrix.
    Uniforms uniforms;
    uniforms.projection = mat4{1};
    // Scale x and y by 1/100.
    uniforms.projection[0][0] = uniforms.projection[1][1] = 1./100.;
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
    wgpuQueueWriteBuffer( webgpu->queue, webgpu->uniforms_buffer, 0, &uniforms, sizeof(Uniforms) );


    // draw the little guys
    int count = 0;
    for (Sprite sprite : sorted_sprites) {
        InstanceData instance;

        auto maybe_tex = engine.resources->get_resource<Texture>(sprite.texture_path);
        if (!maybe_tex.has_value()) { // TODO: fallback texture?
            spdlog::error("Texture {} not found!", sprites[0].texture_path);
            return;
        }

        Texture* tex = maybe_tex.value();
        vec2 scale;
        if( tex->width < tex->height ) {
            scale = vec2( f32(tex->width)/tex->height, 1.0 );
        } else {
            scale = vec2( 1.0, f32(tex->height)/tex->width );
        }

        // spdlog::trace("rendering texture {} of size ({}, {}) at ({}, {}, {}) with scale ({}, {})", 
        //         count,    
        //         tex->width, tex->height,
        //         sprite.position.x, sprite.position.y, sprite.depth,
        //         scale.x * sprite.scale.x, scale.y * sprite.scale.y
        // );
        instance = InstanceData {
            .translation = vec3(sprite.position.x, sprite.position.y, sprite.depth),
            .scale = scale * sprite.scale
        };

        wgpuQueueWriteBuffer( webgpu->queue, instance_buffer, count * sizeof(InstanceData), &instance, sizeof(InstanceData) );

        wgpuRenderPassEncoderSetBindGroup( render_pass, 0, tex->bind_group, 0, nullptr );
        wgpuRenderPassEncoderDraw( render_pass, 4, 1, 0, count );

        count++;
    }

    wgpuRenderPassEncoderEnd(render_pass);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish( encoder, nullptr );
    wgpuQueueSubmit( webgpu->queue, 1, &command_buffer );
    wgpuSurfacePresent( webgpu->surface );

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuTextureViewRelease(current_texture_view);
    wgpuTextureRelease(surface_texture.texture);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(instance_buffer);
}

GraphicsManager::~GraphicsManager() {
    wgpuBindGroupLayoutRelease(webgpu->layout);
    wgpuRenderPipelineRelease(webgpu->pipeline);
    wgpuShaderModuleRelease(webgpu->shader_module);
    wgpuSamplerRelease(webgpu->sampler);
    wgpuBufferRelease(webgpu->uniforms_buffer);
    wgpuBufferRelease(webgpu->vertex_buffer);

    wgpuQueueRelease(webgpu->queue);
    wgpuDeviceRelease(webgpu->device);
    wgpuAdapterRelease(webgpu->adapter);
    wgpuSurfaceRelease(webgpu->surface);
    wgpuInstanceRelease(webgpu->instance);

    glfwDestroyWindow(window);
    glfwTerminate();
}
