#include "rheo_lbm_app.h"

#include "model_runners.h"

namespace rheo {
RheoLBMApp::RheoLBMApp(SimulationModel model) : model_(model) {}

void RheoLBMApp::Run() {
  switch (model_) {
    case SimulationModel::kFreeSurface3D:
      RunFreeSurfaceLbm3DApp();
      break;
    case SimulationModel::kShallowWater2D:
      RunShallowWaterLbm2DApp();
      break;
  }
}
}  // namespace rheo
