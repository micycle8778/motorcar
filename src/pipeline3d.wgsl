struct Uniforms {
    view_projection: mat4x4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) texcoords: vec2f
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texcoords: vec2f
};

@vertex
fn vertex(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.view_projection * vec4f(in.position, 1);
    out.texcoords = in.texcoords;
    return out;
}

@fragment
fn fragment(in: VertexOutput) -> @location(0) vec4f {
    return vec4(in.texcoords.x, in.texcoords.y, 1, 1);
}
