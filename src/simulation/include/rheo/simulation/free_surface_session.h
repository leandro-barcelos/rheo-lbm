#ifndef RHEO_SIMULATION_FREE_SURFACE_SESSION_H
#define RHEO_SIMULATION_FREE_SURFACE_SESSION_H

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

#include "rheo/domain/lattice_settings.h"
#include "rheo/domain/simulation_config.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/simulation/simulation_types.h"

namespace simulation {

enum class SimulationState : std::uint8_t { kEditing, kRunning, kPaused };

struct EditState {
  std::optional<domain::LatticeDefinition> definition;
  std::span<std::uint8_t const> types;
  float min_elevation = 0;
  bool changed = false, can_undo = false, can_redo = false;
} __attribute__((aligned(64)));

struct EditOperation {
  enum class Kind : std::uint8_t { kBrush, kDam, kUndo, kRedo, kRestore };
  Kind kind = Kind::kBrush;
  glm::ivec3 center{};
  domain::BrushSettings brush;
  std::vector<glm::ivec3> points;
} __attribute__((aligned(64)));

class FreeSurfaceSession {
 public:
  FreeSurfaceSession(FreeSurfaceSession&&) = delete;
  FreeSurfaceSession& operator=(FreeSurfaceSession&&) = delete;
  FreeSurfaceSession(graphics::Device const& device,
                     graphics::CommandPools const& command_pools,
                     graphics::FrameSync& frame_sync);
  ~FreeSurfaceSession();

  FreeSurfaceSession(FreeSurfaceSession const&) = delete;
  FreeSurfaceSession& operator=(FreeSurfaceSession const&) = delete;

  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings,
      std::optional<domain::LatticeEdits> const& edits = {});
  [[nodiscard]] EditState Editing() const;
  [[nodiscard]] std::optional<domain::CellHit> Select(
      domain::Ray const& ray) const;
  void BeginStroke();
  void EndStroke();
  std::expected<void, std::string> Edit(EditOperation const& operation);
  [[nodiscard]] std::optional<domain::LatticeEdits> ExportEdits() const;
  std::expected<void, std::string> Play(domain::LbmSettings const& settings);
  void Pause();
  std::expected<void, std::string> ResetToTerrain();
  std::expected<void, std::string> RemoveDam();
  void Clear();
  [[nodiscard]] std::optional<LatticeRenderSnapshot> Update(double delta_ms);
  [[nodiscard]] bool IsRunning() const;
  [[nodiscard]] SimulationState State() const;
  [[nodiscard]] std::uint64_t PhysicalStepCount() const;
  [[nodiscard]] bool CanRemoveDam() const;
  [[nodiscard]] bool IsReady() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace simulation

#endif  // RHEO_SIMULATION_FREE_SURFACE_SESSION_H
