#include "../lattice_types.slang"
[[vk::binding(0, 0)]] StructuredBuffer<float> elevations;
[[vk::binding(1, 0)]] RWStructuredBuffer<Cell> lattice;
struct Parameters {
    uint4 shape;
    uint2 elevation_shape;
    float min_elevation;
    float meters_per_cell;
};
[[vk::push_constant]] Parameters parameters;
[shader("compute")]
[numthreads(4, 4, 4)]
void voxelize_terrain(uint3 id : SV_DispatchThreadID) {
    uint3 shape = parameters.shape.xyz;
    if (any(id >= shape)) return;
    uint index = id.x + id.y * shape.x + id.z * shape.x * shape.y;
    uint2 source = min(uint2((float2(id.xz) + 0.5) / float2(shape.xz) * float2(parameters.elevation_shape)), parameters.elevation_shape - 1);
    float height = max(0.0, elevations[source.x + source.y * parameters.elevation_shape.x] - parameters.min_elevation);
    uint highest = uint(clamp(floor(height / parameters.meters_per_cell), 0.0, float(shape.y - 1)));
    Cell cell = (Cell)0;
    cell.position = float4(float3(id), 0.0);
    cell.type = id.y <= highest ? TERRAIN_OBSTACLE_CELL : GAS_CELL;
    lattice[index] = cell;
}
