#include <limits>
#include <stdexcept>

#include "rheo/domain/simulation_config.h"

void Check(bool value) {
  if (!value) throw std::runtime_error("LBM settings validation failed");
}

int main() {
  domain::SimulationSettingsDraft draft;
  Check(domain::ValidateSimulationSettings(draft).empty());
  for (float omega :
       {0.0F, 2.0F, -1.0F, std::numeric_limits<float>::infinity()}) {
    draft.lbm.omega = omega;
    Check(!domain::ValidateSimulationSettings(draft).empty());
  }
  draft = {};
  draft.lbm.steps_per_second = 0;
  Check(!domain::ValidateSimulationSettings(draft).empty());
  draft = {};
  draft.lbm.lonely_threshold = 1.01F;
  Check(!domain::ValidateSimulationSettings(draft).empty());
  draft = {};
  draft.lbm.fill_offset = -0.01F;
  Check(!domain::ValidateSimulationSettings(draft).empty());
  draft = {};
  draft.lbm.gravity.y = std::numeric_limits<float>::quiet_NaN();
  Check(!domain::ValidateSimulationSettings(draft).empty());
}
