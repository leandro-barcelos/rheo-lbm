#ifndef RHEO_SIMULATION_SHALLOW_WATER_SESSION_H
#define RHEO_SIMULATION_SHALLOW_WATER_SESSION_H

#include "rheo/simulation/shallow_water_solver.h"
namespace simulation {
class ShallowWaterSession {
 public:
  explicit ShallowWaterSession(domain::ShallowWaterScenario scenario);
  ShallowWaterSolver solver;
};
}  // namespace simulation

#endif  // RHEO_SIMULATION_SHALLOW_WATER_SESSION_H
