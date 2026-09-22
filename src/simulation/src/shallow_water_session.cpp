#include "rheo/simulation/shallow_water_session.h"
namespace simulation {
ShallowWaterSession::ShallowWaterSession(domain::ShallowWaterScenario scenario)
    : solver(domain::ShallowWaterParameters(scenario)) {
  solver.Initialize(domain::ShallowWaterInitialState(scenario));
}
}  // namespace simulation
