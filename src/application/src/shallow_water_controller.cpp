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
void ShallowWaterController::Start() {
  if (error_.empty() && Snapshot().steps < Parameters().max_steps)
    running_ = true;
}
void ShallowWaterController::Advance(int steps) {
  if (!running_) return;
  try {
    for (int i = 0; i < steps && Snapshot().steps < Parameters().max_steps; ++i)
      session_->solver.Step();
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
