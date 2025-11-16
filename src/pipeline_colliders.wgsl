struct Uniforms {
    view_projection: mat4x4f,
    directional_light_hat: vec3f,
};

struct InstanceData {
    model_matrix: mat4x4f,
    normal_matrix: mat3x3f,
    albedo: vec4f
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@group(1) @binding(0) var<storage, read> instance_data: InstanceData;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) texcoords: vec2f,
    @location(2) normal: vec3f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) barycentric: vec3f,
};

@vertex
fn vertex(in: VertexInput, @builtin(vertex_index) index: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.view_projection * instance_data.model_matrix * vec4f(in.position, 1);

    // Assign barycentric coordinates based on vertex position in triangle
    // Each vertex of a triangle gets one coordinate as 1, others as 0
    let baryIndex = index % 3u;
    if (baryIndex == 0u) {
        out.barycentric = vec3f(1.0, 0.0, 0.0);
    } else if (baryIndex == 1u) {
        out.barycentric = vec3f(0.0, 1.0, 0.0);
    } else {
        out.barycentric = vec3f(0.0, 0.0, 1.0);
    }
    
    return out;
}

@fragment
fn fragment(in: VertexOutput) -> @location(0) vec4f {
    let deltas = fwidth(in.barycentric);
    let smoothing = deltas * 1.0;
    let thickness = deltas * 0.5;
    
    let d = smoothstep(thickness, thickness + smoothing, in.barycentric);
    let edge_factor = min(min(d.x, d.y), d.z);
    
    // Discard fragments not on edges
    if (edge_factor > 0.5) {
        discard;
    }
    
    // Wireframe color with antialiasing
    let wireframe_color = vec3f(1.0, 1.0, 1.0);
    let alpha = 1.0 - edge_factor * 2.0;
    
    return vec4f(wireframe_color, alpha);
}
