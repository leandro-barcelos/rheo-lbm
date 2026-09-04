#ifndef RHEO_SIMULATION_SIMULATION_SESSION_H
#define RHEO_SIMULATION_SIMULATION_SESSION_H

#include <memory>
#include <optional>

#include "rheo/domain/simulation_config.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/simulation/fluid_types.h"

namespace simulation {

class ISimulationSession {
 public:
  virtual ~ISimulationSession() = default;
  virtual void ApplyConfig(domain::SimulationConfig config) = 0;
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Reset() = 0;
  virtual void Clear() = 0;
  [[nodiscard]] virtual std::optional<FluidRenderSnapshot> Update(
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

  void ApplyConfig(domain::SimulationConfig config) override;
  void Play() override;
  void Pause() override;
  void Reset() override;
  void Clear() override;
  [[nodiscard]] std::optional<FluidRenderSnapshot> Update(
      double delta_ms) override;
  [[nodiscard]] bool IsRunning() const override;
  [[nodiscard]] bool IsReady() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace simulation

#endif  // RHEO_SIMULATION_SIMULATION_SESSION_H
