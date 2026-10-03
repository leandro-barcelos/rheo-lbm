#ifndef RHEO_APPLICATION_SHALLOW_WATER_CONTROLLER_H
#define RHEO_APPLICATION_SHALLOW_WATER_CONTROLLER_H

#include <chrono>
#include <memory>
#include <string>

#include "rheo/simulation/shallow_water_session.h"
namespace application {
using domain::ShallowWaterScenario;
using domain::ShallowWaterSettings;
using domain::ShallowWaterSnapshot;
class ShallowWaterController {
 public:
  ShallowWaterController();
  void Select(ShallowWaterScenario scenario);
  void Reset();
  void Start();
  void Pause() { running_ = false; }
  void Advance(int steps);
  void Update(std::chrono::duration<double, std::milli> budget =
                  std::chrono::milliseconds(8));
  bool Running() const { return running_; }
  const std::string& Error() const { return error_; }
  [[nodiscard]] ShallowWaterSnapshot const & Snapshot() const {
    return session_->Snapshot();
  }
  const ShallowWaterSettings& Parameters() const {
    return session_->Settings();
  }
  ShallowWaterScenario Scenario() const { return scenario_; }

 private:
  ShallowWaterScenario scenario_ = ShallowWaterScenario::Expansion;
  std::unique_ptr<simulation::ShallowWaterSession> session_;
  bool running_ = false;
  std::string error_;
};
}  // namespace application

#endif  // RHEO_APPLICATION_SHALLOW_WATER_CONTROLLER_H
