#include "rheo/simulation/shallow_water_session.h"
namespace simulation {
ShallowWaterSession::ShallowWaterSession(domain::ShallowWaterScenario scenario)
    : solver_(domain::ShallowWaterParameters(scenario)) {
  solver_.Initialize(domain::ShallowWaterInitialState(scenario));
}
}  // namespace simulation
