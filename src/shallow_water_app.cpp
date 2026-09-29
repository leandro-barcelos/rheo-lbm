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
  backend.Init();
  renderer::ShallowWaterRenderer renderer;
  renderer.Init(backend.Device(), backend.CommandPools());
  ui::ShallowWaterInterface ui;
  ui.Init(backend.Window(), backend.GraphicsContext(), backend.Device(),
          backend.SwapChain());
  application::ShallowWaterController controller;
  const char* smoke_frames = std::getenv("RHEO_SMOKE_FRAMES");
  int frame_limit = smoke_frames ? std::atoi(smoke_frames) : 0;
  int frame = 0;
  while (!backend.Window().ShouldClose() &&
         (!frame_limit || frame < frame_limit)) {
    platform::Window::PollEvents();
    (void)backend.InputQueue().Drain();
    auto size = backend.Window().Size();
    if (size.width == 0 || size.height == 0) {
      platform::Window::WaitEvents();
      continue;
    }
    if (size.width != int(backend.SwapChain().Extent().width) ||
        size.height != int(backend.SwapChain().Extent().height)) {
      backend.SwapChain().RecreateSwapChain(backend.Device(), backend.Window());
      ui.OnFrameResourcesChanged(backend.SwapChain().ImageCount());
    }
    controller.Update();
    // Finish prior GPU use before the UI backend updates its buffers.
    backend.Device().LogicalDevice().waitIdle();
    ui.BeginFrame();
    ui.Draw(controller, renderer);
    ui.EndFrame();
    renderer.RenderFrame(backend.Device(), backend.SwapChain(),
                         backend.FrameSync(), backend.Window(), ui);
    ++frame;
  }
  backend.Device().LogicalDevice().waitIdle();
  ui.Shutdown();
}
