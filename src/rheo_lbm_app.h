#ifndef RHEO_LBM_APP_H
#define RHEO_LBM_APP_H

#include <variant>

#include "free_surface_app.h"
#include "rheo/ui/user_interface.h"
#include "shallow_water_app.h"
#include "simulation_model.h"

namespace rheo {

class RheoLBMApp {
 public:
  explicit RheoLBMApp(SimulationModel model = SimulationModel::kFreeSurface3D);
  ~RheoLBMApp();
  RheoLBMApp(RheoLBMApp&&) = delete;
  RheoLBMApp& operator=(RheoLBMApp&&) = delete;
  RheoLBMApp(const RheoLBMApp&) = delete;
  RheoLBMApp& operator=(const RheoLBMApp&) = delete;
  void Run();

 private:
  void UpdateDeltaTime();

  runtime::ApplicationBackend backend_;
  SimulationModel model_;
  ui::UserInterface ui_;
  std::variant<FreeSurfaceApp, ShallowWaterApp> application_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
};
}  // namespace rheo

#endif  // RHEO_LBM_APP_H
