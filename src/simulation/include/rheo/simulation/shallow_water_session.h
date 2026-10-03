#ifndef RHEO_SIMULATION_SHALLOW_WATER_SESSION_H
#define RHEO_SIMULATION_SHALLOW_WATER_SESSION_H

#include "rheo/domain/shallow_water.h"
#include "rheo/simulation/shallow_water_solver.h"
namespace simulation {
class ShallowWaterSession {
 public:
  explicit ShallowWaterSession(domain::ShallowWaterScenario scenario);

  [[nodiscard]] domain::ShallowWaterSnapshot const& Snapshot() const {
    return solver_.Snapshot();
  }
  [[nodiscard]] domain::ShallowWaterSettings const& Settings() const {
    return solver_.Settings();
  }
  void Step() { solver_.Step(); }

 private:
  ShallowWaterSolver solver_;
};
}  // namespace simulation

#endif  // RHEO_SIMULATION_SHALLOW_WATER_SESSION_H
