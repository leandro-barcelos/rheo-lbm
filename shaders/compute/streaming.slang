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

bool diagonal_blocked(int3 source, int i) {
  int3 move = direction(i);
  int axes = (move.x != 0) + (move.y != 0) + (move.z != 0);
  if (axes < 2)
    return false;
  if (move.x != 0 &&
      is_obstacle(lattice[cell_index(source + int3(move.x, 0, 0), parameters.shape.xyz)].type))
    return true;
  if (move.y != 0 &&
      is_obstacle(lattice[cell_index(source + int3(0, move.y, 0), parameters.shape.xyz)].type))
    return true;
  if (move.z != 0 &&
      is_obstacle(lattice[cell_index(source + int3(0, 0, move.z), parameters.shape.xyz)].type))
    return true;
  return false;
}

[shader("compute")]
[numthreads(64, 2, 1)]
void calculate_streaming(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell current = lattice[index];
  float mass = current.mass;
  if (current.type == GAS_CELL || is_obstacle(current.type)) {
    for (int i = 0; i < int(DIRECTION_COUNT); ++i) momentum_write[distribution_index(index, i)] = 0;
    next_mass[index] = mass;
    return;
  }
  momentum_write[distribution_index(index, 0)] =
      momentum_after_collision[distribution_index(index, 0)];
  float eps = epsilon(current);
  float3 normal = surface_normal(int3(id), parameters.shape.xyz, lattice);
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 source_position = int3(id) - direction(i);
    uint target = distribution_index(index, i);
    int inverse = opposite(i);
    if (!valid(source_position, parameters.shape.xyz))
      momentum_write[target] = momentum_after_collision[distribution_index(index, inverse)];
    else {
      uint source_index = cell_index(source_position, parameters.shape.xyz);
      Cell source = lattice[source_index];
      if (is_obstacle(source.type) || diagonal_blocked(source_position, i))
        momentum_write[target] = momentum_after_collision[distribution_index(index, inverse)];
      else if (source.type == GAS_CELL)
        momentum_write[target] = equilibrium(i, parameters.fluid.w, current.velocity.xyz) +
                                 equilibrium(inverse, parameters.fluid.w, current.velocity.xyz) -
                                 momentum_after_collision[distribution_index(index, inverse)];
      else {
        momentum_write[target] = momentum_after_collision[distribution_index(source_index, i)];
        float factor = current.type == FLUID_CELL || source.type == FLUID_CELL
                           ? 1.0
                           : 0.5 * (eps + epsilon(source));
        mass += factor * (momentum_after_collision[distribution_index(source_index, i)] -
                          momentum_after_collision[distribution_index(index, inverse)]);
      }
    }
    if (current.type == INTERFACE_CELL && dot(normal, float3(direction(inverse))) > 0.0)
      momentum_write[target] = equilibrium(i, parameters.fluid.w, current.velocity.xyz) +
                               equilibrium(inverse, parameters.fluid.w, current.velocity.xyz) -
                               momentum_after_collision[distribution_index(index, inverse)];
  }
  next_mass[index] = mass;
}

[shader("compute")]
[numthreads(64, 2, 1)]
void apply_streaming(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell c = lattice[index];
  if (c.type == GAS_CELL || is_obstacle(c.type))
    return;
  c.mass = next_mass[index];
  c.density = 0;
  c.velocity = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    float f = momentum_read[distribution_index(index, i)];
    c.density += f;
    c.velocity += float4(float3(direction(i)) * f, 0);
  }
  if (c.density > MIN_DENSITY) {
    c.velocity /= c.density;
    float speed = length(c.velocity.xyz);
    if (speed > parameters.fluid.z)
      c.velocity *= parameters.fluid.z / speed;
  } else {
    c.density = 0;
    c.velocity = 0;
  }
  c.pressure = c.density / 3.0;
  if (c.type == FLUID_CELL) {
    c.density = c.mass;
    c.pressure = c.density / 3.0;
  }
  lattice[index] = c;
}
