struct Uniforms {
    view_projection: mat4x4f,
    directional_light_hat: vec3f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var texData: texture_2d<f32>;

@group(1) @binding(0) var<storage, read> model_matrix: mat4x4f;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) texcoords: vec2f,
    @location(2) normal: vec3f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texcoords: vec2f,
    @location(1) normal: vec3f,
};

@vertex
fn vertex(in: VertexInput, @builtin(instance_index) index: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.view_projection * model_matrix * vec4f(in.position, 1);
    out.texcoords = in.texcoords;
    out.normal = in.normal;
    return out;
}

@fragment
fn fragment(in: VertexOutput) -> @location(0) vec4f {
    var tex_color = textureSample(texData, texSampler, in.texcoords).rgba;

    var ambient = .1 * tex_color.rgb;
    var diffuse_intensity = max(dot(in.normal, -uniforms.directional_light_hat), 0.f);
    var diffuse = tex_color.rgb * diffuse_intensity;

    return vec4f(ambient + diffuse, tex_color.a);
}
