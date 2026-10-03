#include "rheo/application/shallow_water_controller.h"
namespace application {
ShallowWaterController::ShallowWaterController() { Reset(); }
void ShallowWaterController::Select(ShallowWaterScenario scenario) {
  auto session = std::make_unique<simulation::ShallowWaterSession>(scenario);
  scenario_ = scenario;
  session_ = std::move(session);
  running_ = false;
  error_.clear();
}
void ShallowWaterController::Reset() { Select(scenario_); }
simulation::SimulationStatus ShallowWaterController::Status() const {
  if (!error_.empty()) return simulation::SimulationStatus::kError;
  if (Snapshot().steps >= Parameters().max_steps)
    return simulation::SimulationStatus::kCompleted;
  return running_ ? simulation::SimulationStatus::kRunning
                  : simulation::SimulationStatus::kPaused;
}
simulation::SimulationClock ShallowWaterController::Clock() const {
  auto const& snapshot = Snapshot();
  return {.step_count = snapshot.steps,
          .physical_time_seconds = snapshot.time};
}
void ShallowWaterController::Start() {
  if (error_.empty() && Snapshot().steps < Parameters().max_steps)
    running_ = true;
}
void ShallowWaterController::Advance(int steps) {
  if (!running_) return;
  try {
    for (int i = 0; i < steps && Snapshot().steps < Parameters().max_steps;
         ++i) {
      session_->Step();
    }
  } catch (const std::exception& e) {
    error_ = e.what();
    running_ = false;
  }
  if (Snapshot().steps >= Parameters().max_steps) running_ = false;
}
void ShallowWaterController::Update(
    std::chrono::duration<double, std::milli> budget) {
  auto start = std::chrono::steady_clock::now();
  while (running_ && std::chrono::steady_clock::now() - start < budget)
    Advance(1);
}
}  // namespace application
