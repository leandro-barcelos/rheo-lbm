#include "rheo/simulation/simulation_session.h"

#include <exception>
#include <utility>

#include "lattice_initializer.h"
namespace simulation {
class SimulationSession::Impl {
 public:
  Impl(graphics::Device const& device, graphics::CommandPools const& pools,
       graphics::FrameSync& sync)
      : device_(device), pools_(pools), sync_(sync) {}
  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings) {
    if (!dem) return std::unexpected("Import a valid DEM first");
    auto definition = domain::DefineLattice(*dem, settings);
    if (!definition) return std::unexpected(definition.error());
    try {
      std::uint64_t signal = 0;
      // Older frames must finish before the shared timeline advances on
      // compute.
      device_.LogicalDevice().waitIdle();
      auto next = LatticeInitializer::Build(device_, pools_, sync_, *dem,
                                            *definition, signal);
      lattice_buffer_ = std::move(next);
      snapshot_ = LatticeRenderSnapshot{
          .lattice_buffer = {.native_handle = reinterpret_cast<std::uintptr_t>(
                                 static_cast<VkBuffer>(
                                     *lattice_buffer_.buffer))},
          .lattice_width = definition->width,
          .lattice_height = definition->height,
          .lattice_depth = definition->depth,
          .cell_count = definition->cell_count,
          .terrain_elevation_cells = definition->terrain_elevation_cells,
          .ready_signal = signal};
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not initialize terrain: ") +
                             error.what());
    }
  }
  void Clear() {
    device_.LogicalDevice().waitIdle();
    snapshot_.reset();
    lattice_buffer_ = {};
  }
  graphics::Device const& device_;
  graphics::CommandPools const& pools_;
  graphics::FrameSync& sync_;
  graphics::AllocatedBuffer lattice_buffer_;
  std::optional<LatticeRenderSnapshot> snapshot_;
};
SimulationSession::SimulationSession(graphics::Device const& device,
                                     graphics::CommandPools const& pools,
                                     graphics::FrameSync& sync)
    : impl_(std::make_unique<Impl>(device, pools, sync)) {}
SimulationSession::~SimulationSession() = default;
std::expected<void, std::string> SimulationSession::InitializeTerrain(
    domain::SharedDem dem, domain::LatticeSettings settings) {
  return impl_->InitializeTerrain(std::move(dem), settings);
}
void SimulationSession::Clear() { impl_->Clear(); }
void SimulationSession::Play() {
}  // Fluid dynamics will be implemented in the next stage.
void SimulationSession::Pause() {}
bool SimulationSession::IsRunning() const { return false; }
bool SimulationSession::IsReady() const { return impl_->snapshot_.has_value(); }
std::optional<LatticeRenderSnapshot> SimulationSession::Update(double) {
  return impl_->snapshot_;
}
}  // namespace simulation
