#pragma once
#include "rheo/simulation/labswe_solver.h"
namespace simulation {
class ShallowWaterSession {
 public:
  explicit ShallowWaterSession(domain::ShallowWaterScenario scenario);
  LabsweSolver solver;
};
}  // namespace simulation
