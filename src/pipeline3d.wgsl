struct Uniforms {
    view_projection: mat4x4f,
    camera_pos: vec3f,
};

struct LightData {
    ambient: vec3f,
    diffuse: vec3f,
    specular: vec3f,

    position: vec3f,
    distance: f32
};

struct InstanceData {
    model_matrix: mat4x4f,
    normal_matrix: mat3x3f,
    albedo: vec4f
};

const NUM_LIGHTS: u32 = 8;

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var texSampler: sampler;
@group(0) @binding(2) var texData: texture_2d<f32>;
@group(0) @binding(3) var<uniform> lights: array<LightData, NUM_LIGHTS>;

@group(1) @binding(0) var<storage, read> instance_data: InstanceData;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) texcoords: vec2f,
    @location(2) normal: vec3f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texcoords: vec2f,
    @location(1) normal: vec3f,
    @location(2) world_coords: vec3f,
};

@vertex
fn vertex(in: VertexInput, @builtin(instance_index) index: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.view_projection * instance_data.model_matrix * vec4f(in.position, 1);
    out.texcoords = in.texcoords;
    out.normal = normalize(instance_data.normal_matrix * in.normal);
    out.world_coords = (instance_data.model_matrix * vec4f(in.position, 1.)).xyz;
    return out;
}

@fragment
fn fragment(in: VertexOutput) -> @location(0) vec4f {
    let normal = normalize(in.normal);
    var tex_color = textureSample(texData, texSampler, in.texcoords).rgba * instance_data.albedo;
    if (tex_color.a < 0.1) { discard; }

    var ret = vec3f(.05);
    for (var idx = 0u; idx < NUM_LIGHTS; idx = idx + 1u) {
        let light = lights[idx];

        let attenuation = max(1f - (distance(in.world_coords, light.position) / light.distance), 0f);
        let light_dir = normalize(light.position - in.world_coords);
        let view_dir = normalize(uniforms.camera_pos - in.world_coords);

        let diffuse_power = max(dot(normal, light_dir), 0.);
        let a = light.ambient.rgb * attenuation;
        let d = light.diffuse.rgb * diffuse_power * attenuation;
        // TODO: add specular materials
        let s = light.specular.rgb * pow(max(dot(normal, normalize(light_dir + view_dir)), 0.), 4.) * attenuation;

        ret += d;
        if (diffuse_power > 0.) {
            ret += s;
        }

        ret = max(ret, a);
    }

    // return vec4f(ret * tex_color.rgb, tex_color.a);
    return vec4f(pow(ret * tex_color.rgb, vec3f(1. / 2.2)), tex_color.a);
}
