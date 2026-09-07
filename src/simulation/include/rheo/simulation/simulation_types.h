#ifndef RHEO_SIMULATION_SIMULATION_TYPES_H
#define RHEO_SIMULATION_SIMULATION_TYPES_H

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

#include "rheo/domain/lattice_editing.h"
#include "rheo/graphics/buffer_view.h"

namespace simulation {

using domain::CellType;

struct Cell {  // NOLINT
  glm::vec4 position;
  glm::vec4 velocity;
  CellType type;
  float density;
  float pressure;
  float mass;
} __attribute__((aligned(16)));

struct LatticeRenderSnapshot {
  graphics::GraphicsBufferView lattice_buffer;
  std::uint32_t lattice_width = 0;
  std::uint32_t lattice_height = 0;
  std::uint32_t lattice_depth = 0;
  std::uint32_t cell_count = 0;
  float terrain_elevation_cells = 0.0F;
  std::uint64_t ready_signal = 0;
} __attribute__((aligned(32)));

}  // namespace simulation

static_assert(sizeof(simulation::Cell) == 48);

static_assert(sizeof(simulation::CellType) == 4);
static_assert(offsetof(simulation::Cell, position) == 0);
static_assert(offsetof(simulation::Cell, velocity) == 16);
static_assert(offsetof(simulation::Cell, type) == 32);
static_assert(offsetof(simulation::Cell, density) == 36);
static_assert(offsetof(simulation::Cell, pressure) == 40);
static_assert(offsetof(simulation::Cell, mass) == 44);

#endif  // RHEO_SIMULATION_SIMULATION_TYPES_H
