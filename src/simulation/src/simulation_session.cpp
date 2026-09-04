#include "rheo/simulation/simulation_session.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "fluid_simulator.h"

namespace simulation {

class SimulationSession::Impl {
 public:
  Impl(graphics::Device const& device,
       graphics::CommandPools const& command_pools,
       graphics::FrameSync& frame_sync)
      : device_(device),
        command_pools_(command_pools),
        frame_sync_(frame_sync) {}

  void ApplyConfig(domain::SimulationConfig config) {
    if (simulator_ != nullptr) {
      device_.LogicalDevice().waitIdle();
      simulator_.reset();
    }
    config_ = std::move(config);
    config_dirty_ = true;
    running_ = false;
    ready_signal_ = 0;
  }

  void Play() {
    if (config_dirty_) {
      Recreate();
    }
    running_ = simulator_ != nullptr;
  }

  void Pause() {
    running_ = false;
    ready_signal_ = 0;
  }

  void Reset() {
    Recreate();
    running_ = false;
    ready_signal_ = 0;
  }

  void Clear() {
    device_.LogicalDevice().waitIdle();
    simulator_.reset();
    config_.reset();
    running_ = false;
    config_dirty_ = false;
    ready_signal_ = 0;
  }

  std::optional<FluidRenderSnapshot> Update(double delta_ms) {
    if (running_ && simulator_ != nullptr) {
      ready_signal_ = simulator_->Run(device_, frame_sync_, delta_ms);
    }
    if (simulator_ == nullptr) {
      return std::nullopt;
    }
    return FluidRenderSnapshot{
        .particle_buffer =
            {.native_handle =
                 reinterpret_cast<std::uintptr_t>(static_cast<VkBuffer>(
                     *simulator_->FluidParticlesReadBuffer().buffer))},
        .particle_count = simulator_->FluidParticleCount(),
        .ready_signal = ready_signal_,
    };
  }

  [[nodiscard]] bool IsRunning() const { return running_; }
  [[nodiscard]] bool IsReady() const { return simulator_ != nullptr; }

 private:
  void Recreate() {
    if (!config_) {
      return;
    }
    device_.LogicalDevice().waitIdle();
    simulator_.reset();
    simulator_ = std::make_unique<FluidSimulator>(*config_);
    simulator_->Init(device_, command_pools_);
    config_dirty_ = false;
  }

  graphics::Device const& device_;
  graphics::CommandPools const& command_pools_;
  graphics::FrameSync& frame_sync_;
  std::unique_ptr<FluidSimulator> simulator_;
  std::optional<domain::SimulationConfig> config_;
  std::uint64_t ready_signal_ = 0;
  bool running_ = false;
  bool config_dirty_ = false;
};

SimulationSession::SimulationSession(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    graphics::FrameSync& frame_sync)
    : impl_(std::make_unique<Impl>(device, command_pools, frame_sync)) {}

SimulationSession::~SimulationSession() = default;

void SimulationSession::ApplyConfig(domain::SimulationConfig config) {
  impl_->ApplyConfig(std::move(config));
}

void SimulationSession::Play() { impl_->Play(); }
void SimulationSession::Pause() { impl_->Pause(); }
void SimulationSession::Reset() { impl_->Reset(); }
void SimulationSession::Clear() { impl_->Clear(); }

std::optional<FluidRenderSnapshot> SimulationSession::Update(double delta_ms) {
  return impl_->Update(delta_ms);
}

bool SimulationSession::IsRunning() const { return impl_->IsRunning(); }
bool SimulationSession::IsReady() const { return impl_->IsReady(); }

}  // namespace simulation
