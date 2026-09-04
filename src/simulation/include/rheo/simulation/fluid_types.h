#ifndef RHEO_SIMULATION_FLUID_TYPES_H
#define RHEO_SIMULATION_FLUID_TYPES_H

#include <cstdint>
#include <glm/glm.hpp>

#include "rheo/graphics/buffer_view.h"

namespace simulation {

struct FluidParticle {
  glm::vec4 position;
  glm::vec4 velocity;
  glm::vec4 distance_traveled;
  glm::vec4 color;
  float density;
} __attribute__((aligned(16)));

struct FluidRenderSnapshot {
  graphics::GraphicsBufferView particle_buffer;
  std::uint32_t particle_count = 0;
  std::uint64_t ready_signal = 0;
};

}  // namespace simulation

static_assert(sizeof(simulation::FluidParticle) == 80);

#endif  // RHEO_SIMULATION_FLUID_TYPES_H
