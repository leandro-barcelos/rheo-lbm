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
void mark_transitions(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  transition_mask[index] = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i)
    excess_distributions[distribution_index(index, i)] = 0;
  Cell c = lattice[index];
  if (c.type != INTERFACE_CELL)
    return;
  bool has_fluid = false, has_gas = false, has_interface = false;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 p = int3(id) + direction(i);
    if (!valid(p, parameters.shape.xyz))
      continue;
    int t = lattice[cell_index(p, parameters.shape.xyz)].type;
    has_fluid |= t == FLUID_CELL;
    has_gas |= t == GAS_CELL;
    has_interface |=
        t == INTERFACE_CELL || t == INTERFACE_TO_FLUID_CELL || t == INTERFACE_TO_GAS_CELL;
  }
  if (c.mass > (1.0 + parameters.interface_values.x) * c.density ||
      (c.mass >= (1.0 - parameters.interface_values.y) * c.density && !has_gas))
    lattice[index].type = INTERFACE_TO_FLUID_CELL;
  else if (c.mass < -parameters.interface_values.x * c.density ||
           (c.mass <= parameters.interface_values.y * c.density && !has_fluid) ||
           (!has_interface && !has_fluid))
    lattice[index].type = INTERFACE_TO_GAS_CELL;
}

[shader("compute")]
[numthreads(64, 2, 1)]
void update_fluid_transition_neighbors(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell c = lattice[index];
  if (c.type == INTERFACE_TO_FLUID_CELL)
    return;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 source = int3(id) - direction(i);
    if (!valid(source, parameters.shape.xyz))
      continue;
    if (lattice[cell_index(source, parameters.shape.xyz)].type != INTERFACE_TO_FLUID_CELL)
      continue;
    if (c.type == GAS_CELL)
      transition_mask[index] = 1;
    else if (c.type == INTERFACE_TO_GAS_CELL)
      lattice[index].type = INTERFACE_CELL;
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void apply_gas_to_interface(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  if (transition_mask[index] == 0)
    return;
  Cell c = lattice[index];
  c.type = INTERFACE_CELL;
  float density = 0;
  float3 velocity = 0;
  int count = 0;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 p = int3(id) + direction(i);
    if (!valid(p, parameters.shape.xyz))
      continue;
    uint n = cell_index(p, parameters.shape.xyz);
    Cell neighbor = lattice[n];
    if (transition_mask[n] == 0 &&
        (neighbor.type == FLUID_CELL || neighbor.type == INTERFACE_CELL)) {
      density += neighbor.density;
      velocity += neighbor.velocity.xyz;
      count++;
    }
  }
  c.density = count > 0 ? density / count : parameters.fluid.w;
  c.velocity = count > 0 ? float4(velocity / count, 0) : float4(0, 0, 0, 0);
  c.mass = 0;
  lattice[index] = c;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    uint d = distribution_index(index, i);
    float f = equilibrium(i, c.density, c.velocity.xyz);
    momentum_read[d] = f;
    momentum_after_collision[d] = f;
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void update_gas_transition_neighbors(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  transition_mask[index] = 0;
  if (lattice[index].type != FLUID_CELL)
    return;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 source = int3(id) - direction(i);
    if (valid(source, parameters.shape.xyz) &&
        lattice[cell_index(source, parameters.shape.xyz)].type == INTERFACE_TO_GAS_CELL)
      lattice[index].type = INTERFACE_CELL;
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void calculate_excess_distribution(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  Cell c = lattice[index];
  if (c.type != INTERFACE_TO_FLUID_CELL && c.type != INTERFACE_TO_GAS_CELL)
    return;
  float3 n = surface_normal(int3(id), parameters.shape.xyz, lattice);
  float excess = c.type == INTERFACE_TO_FLUID_CELL ? c.mass - c.density : c.mass;
  if (c.type == INTERFACE_TO_GAS_CELL)
    n = -n;
  float target[19];
  float total = 0;
  int count = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    target[i] = -1;
    if (i == 0)
      continue;
    int3 p = int3(id) + direction(i);
    if (!valid(p, parameters.shape.xyz))
      continue;
    if (lattice[cell_index(p, parameters.shape.xyz)].type == INTERFACE_CELL) {
      float w = max(dot(float3(direction(i)), n), 0.0);
      target[i] = w;
      total += w;
      count++;
    }
  }
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    if (target[i] < 0)
      continue;
    int3 p = int3(id) + direction(i);
    float share = excess * (total > 0 ? target[i] / total : 1.0 / count);
    excess_distributions[distribution_index(index, i)] = share;
    if (share != 0)
      InterlockedOr(transition_mask[cell_index(p, parameters.shape.xyz)], 1);
  }
  lattice[index].mass = c.type == INTERFACE_TO_FLUID_CELL ? c.density : 0;
}

[shader("compute")]
[numthreads(64, 2, 1)]
void apply_excess_distribution(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  if (lattice[index].type != INTERFACE_CELL || transition_mask[index] == 0)
    return;
  float incoming = 0;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 source = int3(id) - direction(i);
    if (valid(source, parameters.shape.xyz))
      incoming +=
          excess_distributions[distribution_index(cell_index(source, parameters.shape.xyz), i)];
  }
  lattice[index].mass += incoming;
}

[shader("compute")]
[numthreads(64, 2, 1)]
void apply_transitions(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  int type = lattice[index].type;
  if (type == INTERFACE_TO_FLUID_CELL)
    lattice[index].type = FLUID_CELL;
  else if (type == INTERFACE_TO_GAS_CELL) {
    lattice[index].type = GAS_CELL;
    lattice[index].density = 0;
    lattice[index].velocity = 0;
    for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
      uint d = distribution_index(index, i);
      momentum_read[d] = 0;
      momentum_after_collision[d] = 0;
    }
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void remove_dam(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  if (lattice[index].type != DAM_OBSTACLE_CELL)
    return;
  lattice[index].type = GAS_CELL;
  lattice[index].velocity = 0;
  lattice[index].density = 0;
  lattice[index].pressure = 0;
  lattice[index].mass = 0;
  next_mass[index] = 0;
  transition_mask[index] = 0;
  for (int i = 0; i < int(DIRECTION_COUNT); ++i) {
    uint d = distribution_index(index, i);
    momentum_read[d] = 0;
    momentum_write[d] = 0;
    momentum_after_collision[d] = 0;
    excess_distributions[d] = 0;
  }
}

[shader("compute")]
[numthreads(64, 2, 1)]
void reclassify_after_dam_removal(uint3 id: SV_DispatchThreadID) {
  if (any(id >= parameters.shape.xyz))
    return;
  uint index = cell_index(int3(id), parameters.shape.xyz);
  if (lattice[index].type != FLUID_CELL)
    return;
  for (int i = 1; i < int(DIRECTION_COUNT); ++i) {
    int3 neighbor = int3(id) + direction(i);
    if (!valid(neighbor, parameters.shape.xyz))
      continue;
    if (lattice[cell_index(neighbor, parameters.shape.xyz)].type != GAS_CELL)
      continue;
    lattice[index].type = INTERFACE_CELL;
    lattice[index].mass = 0.5 * lattice[index].density;
    next_mass[index] = lattice[index].mass;
    return;
  }
}
