#ifndef RHEO_LBM_APP_H
#define RHEO_LBM_APP_H

#include <cstdint>
namespace rheo {
enum class SimulationModel : uint8_t { kFreeSurface3D, kShallowWater2D };

class RheoLBMApp {
 public:
  explicit RheoLBMApp(SimulationModel model = SimulationModel::kFreeSurface3D);
  ~RheoLBMApp() = default;
  RheoLBMApp(RheoLBMApp&&) = delete;
  RheoLBMApp& operator=(RheoLBMApp&&) = delete;
  RheoLBMApp(const RheoLBMApp&) = delete;
  RheoLBMApp& operator=(const RheoLBMApp&) = delete;
  void Run();

 private:
  SimulationModel model_;
};
}  // namespace rheo

#endif  // RHEO_LBM_APP_H
