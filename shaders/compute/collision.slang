#include "common.slang"
[[vk::binding(0, 0)]]
RWStructuredBuffer<Cell> lattice;
[[vk::binding(1, 0)]]
RWStructuredBuffer<float> momentum_read;
[[vk::binding(2, 0)]]
RWStructuredBuffer<float> momentum_write;
[[vk::binding(3, 0)]]
RWStructuredBuffer<float> momentum_after_collision;
[[vk::binding(4, 0)]]
RWStructuredBuffer<float> next_mass;
[[vk::binding(5, 0)]]
RWStructuredBuffer<uint> transition_mask;
[[vk::binding(6, 0)]]
RWStructuredBuffer<float> excess_distributions;
struct Parameters {
  uint4 shape;
  float4 fluid;
  float4 interface_values;
  float4 gravity;
};
[[vk::push_constant]]
Parameters parameters;

[shader("compute")]
[numthreads(64, 2, 1)]
void initialize_lbm(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell c = lattice[index];
  bool liquid = c.type == FLUID_CELL || c.type == INTERFACE_CELL;
  c.velocity = 0;
  c.density = liquid ? parameters.fluid.x : 0.0;
  c.pressure = c.density / 3.0;
  c.mass = c.type == FLUID_CELL ? c.density : c.type == INTERFACE_CELL ? 0.5 * c.density : 0.0;
  lattice[index] = c;
  next_mass[index] = c.mass;
  transition_mask[index] = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    uint d = distribution_index(index, i);
    float f = equilibrium(i, c.density, c.velocity.xyz);
    momentum_read[d] = f;
    momentum_write[d] = f;
    momentum_after_collision[d] = f;
    excess_distributions[d] = 0;
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void collide(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell c = lattice[index];
  if (c.type == GAS_CELL || is_obstacle(c.type)) {
    for (int i = 0; i < int(DIRECTION_COUNT); ++i)
      momentum_after_collision[distribution_index(index, i)] = 0;
    return;
  }
  float density = 0;
  float3 momentum = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    float f = momentum_read[distribution_index(index, i)];
    density += f;
    momentum += float3(direction(i)) * f;
  }
  float3 velocity = 0;
  if (density > MIN_DENSITY) {
    velocity = momentum / density;
    float speed = length(velocity);
    if (speed > parameters.fluid.z)
      velocity *= parameters.fluid.z / speed;
  } else
    density = 0;
  c.density = density;
  c.velocity = float4(velocity, 0);
  c.pressure = density / 3.0;
  lattice[index] = c;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    uint d = distribution_index(index, i);
    float f = momentum_read[d];
    float force = density * weight(i) * dot(float3(direction(i)), parameters.gravity.xyz);
    momentum_after_collision[d] = (1.0 - parameters.fluid.y) * f +
                                  parameters.fluid.y * equilibrium(i, density, velocity) + force;
  }
}
