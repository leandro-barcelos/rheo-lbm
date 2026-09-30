#include <algorithm>
#include <cstdlib>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "shallow_water_app.h"

namespace rheo {
ShallowWaterApp::ShallowWaterApp(runtime::ApplicationBackend& backend) {
#ifdef _OPENMP
  if (std::getenv("OMP_NUM_THREADS") == nullptr) { //NOLINT
    omp_set_num_threads(std::min(4, omp_get_max_threads()));
}
#endif

  renderer_.Init(backend.Device(), backend.CommandPools());
  ui_.Init(backend.Window(), backend.GraphicsContext(), backend.Device(),
           backend.SwapChain());
}

ShallowWaterApp::~ShallowWaterApp() {
  ui_.Shutdown();
}

bool ShallowWaterApp::Update(runtime::ApplicationBackend& backend,
                             double /*delta_time*/) {
  (void)backend.InputQueue().Drain();

  if (backend.RecreteSwapChain()) {
    ui_.OnFrameResourcesChanged(backend.SwapChain().ImageCount());
  }

  application_.Update();
  backend.WaitIdle();
  ui_.BeginFrame();
  ui_.Draw(application_, renderer_);
  ui_.EndFrame();
  renderer_.RenderFrame(backend.Device(), backend.SwapChain(),
                        backend.FrameSync(), backend.Window(), ui_);

  return backend.ShouldClose();
}


}  // namespace rheo
