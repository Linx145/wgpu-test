struct VertexOutputs {
    //The position of the vertex
    @builtin(position) position: vec4<f32>,
    //The texture cooridnate of the vertex
    @location(0) tex_coord: vec2<f32>
}

struct FragmentInputs {
    @location(0) tex_coord: vec2<f32>
}

@vertex
fn vs_main(
    @builtin(vertex_index) VertexIndex : u32
) -> VertexOutputs {
    var output: VertexOutputs;

    var pos = array<vec2<f32>, 4> (
      vec2<f32>(-0.5, -0.5),
      vec2<f32>(0.5, -0.5),
      vec2<f32>(0.5, 0.5),
      vec2<f32>(-0.5, 0.5)
    );

    var UVs = array<vec2<f32>, 4> (
      vec2<f32>(0.0, 0.0),
      vec2<f32>(1.0, 0.0),
      vec2<f32>(1.0, 1.0),
      vec2<f32>(0.0, 1.0)
    );

    output.position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
    output.tex_coord = UVs[VertexIndex];

    return output;
}

//The texture we're sampling
@group(0) @binding(0) var t: binding_array<texture_2d<f32>, 2>;
//The sampler we're using to sample the texture
@group(0) @binding(1) var s: sampler;

@fragment
fn fs_main(input: FragmentInputs) -> @location(0) vec4<f32> {
    return textureSample(t[0], s, input.tex_coord);
}