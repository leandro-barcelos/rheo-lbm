#include <algorithm>
#include <cstdlib>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "shallow_water_app.h"

namespace rheo {
ShallowWaterApp::ShallowWaterApp(runtime::ApplicationBackend& backend) {
#ifdef _OPENMP
  if (std::getenv("OMP_NUM_THREADS") == nullptr) {  // NOLINT
    omp_set_num_threads(std::min(4, omp_get_max_threads()));
  }
#endif

  renderer_.Init(backend.Device(), backend.CommandPools());
}

bool ShallowWaterApp::Update(runtime::ApplicationBackend& backend,
                             double /*delta_time*/,
                             ui::UserInterface& user_interface) {
  (void)backend.InputQueue().Drain();

  if (backend.RecreteSwapChain()) {
    user_interface.OnFrameResourcesChanged(backend.SwapChain().ImageCount());
  }

  application_.Update();
  backend.WaitIdle();
  user_interface.BeginFrame();
  user_interface.Draw(application_, renderer_);
  user_interface.EndFrame();
  renderer_.RenderFrame(backend.Device(), backend.SwapChain(),
                        backend.FrameSync(), backend.Window(), user_interface);

  return backend.ShouldClose();
}

}  // namespace rheo
