#include "../lattice_types.slang"
// GLM uploads column-major matrices.
struct Camera { column_major float4x4 model; column_major float4x4 view; column_major float4x4 proj; };
[[vk::binding(0,0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1,0)]] StructuredBuffer<Cell> lattice;
struct Parameters { float4 shape_elevation; };
[[vk::push_constant]] Parameters parameters;
struct VertexOutput { float4 position : SV_Position; float gray : COLOR0; };
[shader("vertex")]
VertexOutput vert_main(uint vertex_id : SV_VulkanVertexID, uint instance_id : SV_VulkanInstanceID) {
    VertexOutput output;
    Cell cell = lattice[instance_id];
    output.gray = saturate(cell.position.y / max(parameters.shape_elevation.w, 1.0));
    if (cell.type != TERRAIN_OBSTACLE_CELL) {
        output.position = float4(2,2,2,1);
        return output;
    }
    // Corner bits are X, Y, Z. Two triangles per cube face.
    static const uint corners[36] = {
        0,4,2, 2,4,6, 1,3,5, 3,7,5,
        0,1,4, 1,5,4, 2,6,3, 3,6,7,
        0,2,1, 1,2,3, 4,5,6, 5,7,6
    };
    uint corner = corners[vertex_id];
    float3 offset = float3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1) - 0.5;
    float3 shape = parameters.shape_elevation.xyz;
    float scale = 1.0 / max(max(shape.x, shape.y), shape.z);
    float3 position = (cell.position.xyz - 0.5 * (shape - 1.0) + offset) * scale;
    output.position = mul(camera.proj, mul(camera.view, mul(camera.model, float4(position,1))));
    return output;
}
[shader("fragment")]
float4 frag_main(VertexOutput input) : SV_Target { return float4(input.gray.xxx,1); }
