#ifndef RHEO_SIMULATION_LATTICE_INITIALIZER_H
#define RHEO_SIMULATION_LATTICE_INITIALIZER_H
#include "rheo/domain/lattice_editing.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/frame_sync.h"
namespace simulation {
class LatticeInitializer {
 public:
  // Waits for initialization before releasing temporary descriptors and
  // elevations.
  static graphics::AllocatedBuffer Build(
      graphics::Device const& device, graphics::CommandPools const& pools,
      graphics::FrameSync& sync, domain::DemData const& dem,
      domain::LatticeDefinition const& definition, std::uint64_t& ready_signal,
      domain::CellTypes& types);
};
}  // namespace simulation
#endif
