#ifndef RHEO_LBM_APP_H
#define RHEO_LBM_APP_H

#include <cstdint>
#include <variant>

#include "free_surface_app.h"
#include "shallow_water_app.h"

namespace rheo {
enum class SimulationModel : uint8_t { kFreeSurface3D, kShallowWater2D };

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
  std::variant<FreeSurfaceApp, ShallowWaterApp> application_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
};
}  // namespace rheo

#endif  // RHEO_LBM_APP_H
