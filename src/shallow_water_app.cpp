#include <algorithm>
#include <cstdlib>

#include "model_runners.h"
#include "rheo/renderer/shallow_water_renderer.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/ui/shallow_water_interface.h"
#ifdef _OPENMP
#include <omp.h>
#endif
void rheo::RunShallowWaterApp() {
#ifdef _OPENMP
  if (!std::getenv("OMP_NUM_THREADS"))
    omp_set_num_threads(std::min(4, omp_get_max_threads()));
#endif
  constexpr platform::WindowProperties kWindowProperties{
      .width = 1280, .height = 720, .title = "Rheo LBM"};
  runtime::ApplicationBackend backend{runtime::ApplicationBackend::Config{
      .window_properties = kWindowProperties}};
  renderer::ShallowWaterRenderer renderer;
  renderer.Init(backend.Device(), backend.CommandPools());
  ui::ShallowWaterInterface ui;
  ui.Init(backend.Window(), backend.GraphicsContext(), backend.Device(),
          backend.SwapChain());
  application::ShallowWaterController controller;
  while (!backend.ShouldClose()) {
    runtime::ApplicationBackend::PollEvents();
    if (backend.IsMinimized()) {
      runtime::ApplicationBackend::WaitEvents();
      continue;
    }

    (void)backend.InputQueue().Drain();

    if (backend.RecreteSwapChain()) {
      ui.OnFrameResourcesChanged(backend.SwapChain().ImageCount());
    }

    controller.Update();
    backend.WaitIdle();
    ui.BeginFrame();
    ui.Draw(controller, renderer);
    ui.EndFrame();
    renderer.RenderFrame(backend.Device(), backend.SwapChain(),
                         backend.FrameSync(), backend.Window(), ui);
  }
  ui.Shutdown();
}
