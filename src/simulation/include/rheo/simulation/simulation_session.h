#ifndef RHEO_SIMULATION_SIMULATION_SESSION_H
#define RHEO_SIMULATION_SIMULATION_SESSION_H

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
struct EditState {
  std::optional<domain::LatticeDefinition> definition;
  std::span<std::uint8_t const> types;
  float min_elevation = 0;
  bool changed = false, can_undo = false, can_redo = false;
};
struct EditOperation {
  enum class Kind { kBrush, kDam, kUndo, kRedo, kRestore } kind = Kind::kBrush;
  glm::ivec3 center{};
  domain::BrushSettings brush;
  std::vector<glm::ivec3> points;
};

class ISimulationSession {
 public:
  virtual ~ISimulationSession() = default;
  virtual std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings,
      std::optional<domain::LatticeEdits> const& edits = {}) = 0;
  virtual EditState Editing() const = 0;
  [[nodiscard]] std::optional<domain::CellHit> Select(
      domain::Ray const& ray) const {
    auto state = Editing();
    return state.definition
               ? domain::PickCell(ray, *state.definition, state.types)
               : std::nullopt;
  }
  virtual void BeginStroke() = 0;
  virtual void EndStroke() = 0;
  virtual std::expected<void, std::string> Edit(
      EditOperation const& operation) = 0;
  virtual std::optional<domain::LatticeEdits> ExportEdits() const = 0;
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Clear() = 0;
  [[nodiscard]] virtual std::optional<LatticeRenderSnapshot> Update(
      double delta_ms) = 0;
  [[nodiscard]] virtual bool IsRunning() const = 0;
  [[nodiscard]] virtual bool IsReady() const = 0;
};

class SimulationSession final : public ISimulationSession {
 public:
  SimulationSession(graphics::Device const& device,
                    graphics::CommandPools const& command_pools,
                    graphics::FrameSync& frame_sync);
  ~SimulationSession() override;

  SimulationSession(SimulationSession const&) = delete;
  SimulationSession& operator=(SimulationSession const&) = delete;

  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings,
      std::optional<domain::LatticeEdits> const& edits = {}) override;
  EditState Editing() const override;
  void BeginStroke() override;
  void EndStroke() override;
  std::expected<void, std::string> Edit(
      EditOperation const& operation) override;
  std::optional<domain::LatticeEdits> ExportEdits() const override;
  void Play() override;
  void Pause() override;
  void Clear() override;
  [[nodiscard]] std::optional<LatticeRenderSnapshot> Update(
      double delta_ms) override;
  [[nodiscard]] bool IsRunning() const override;
  [[nodiscard]] bool IsReady() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace simulation

#endif  // RHEO_SIMULATION_SIMULATION_SESSION_H
