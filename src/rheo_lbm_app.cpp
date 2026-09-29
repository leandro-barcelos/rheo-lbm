#include "rheo_lbm_app.h"

#include "model_runners.h"

namespace rheo {
RheoLBMApp::RheoLBMApp(SimulationModel model) : model_(model) {}

void RheoLBMApp::Run() {
  switch (model_) {
    case SimulationModel::kFreeSurface3D:
      RunFreeSurfaceApp();
      break;
    case SimulationModel::kShallowWater2D:
      RunShallowWaterApp();
      break;
  }
}
}  // namespace rheo
