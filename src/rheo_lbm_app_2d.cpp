#include <algorithm>
#include <cstdlib>

#include "rheo/events/input_queue.h"
#include "rheo/graphics/context.h"
#include "rheo/renderer/shallow_water_renderer.h"
#include "rheo/ui/shallow_water_interface.h"
#include "rheo_lbm_app.h"
#ifdef _OPENMP
#include <omp.h>
#endif
namespace rheo {
class RheoLBMApp::Impl {
 public:
  void Run() {
#ifdef _OPENMP
    if (!std::getenv("OMP_NUM_THREADS"))
      omp_set_num_threads(std::min(4, omp_get_max_threads()));
#endif
    events::InputQueue input;
    platform::Window window({1280, 800, "Rheo LBM - shallow water 2D"}, input);
    graphics::GraphicsContext context;
    context.Init(platform::Window::RequiredGraphicsExtensions());
    context.CreateSurface(window);
    graphics::Device device;
    device.Init(context, *context.Surface());
    graphics::SwapChain swap;
    swap.Init(device, *context.Surface(), window);
    graphics::CommandPools pools;
    pools.Init(device);
    graphics::FrameSync sync;
    sync.Init(device);
    renderer::ShallowWaterRenderer renderer;
    renderer.Init(device, pools);
    ui::ShallowWaterInterface ui;
    ui.Init(window, context, device, swap);
    application::ShallowWaterController controller;
    const char* smoke_frames = std::getenv("RHEO_SMOKE_FRAMES");
    int frame_limit = smoke_frames ? std::atoi(smoke_frames) : 0;
    int frame = 0;
    while (!window.ShouldClose() && (!frame_limit || frame < frame_limit)) {
      platform::Window::PollEvents();
      (void)input.Drain();
      auto size = window.Size();
      if (size.width == 0 || size.height == 0) {
        platform::Window::WaitEvents();
        continue;
      }
      if (size.width != int(swap.Extent().width) ||
          size.height != int(swap.Extent().height)) {
        swap.RecreateSwapChain(device, window);
        ui.OnFrameResourcesChanged(swap.ImageCount());
      }
      controller.Update();
      // Finish prior GPU use before the UI backend updates its buffers.
      device.LogicalDevice().waitIdle();
      ui.BeginFrame();
      ui.Draw(controller, renderer);
      ui.EndFrame();
      renderer.RenderFrame(device, swap, sync, window, ui);
      ++frame;
    }
    device.LogicalDevice().waitIdle();
    ui.Shutdown();
  }
};
}  // namespace rheo

rheo::RheoLBMApp::RheoLBMApp() : impl_(std::make_unique<Impl>()) {}
rheo::RheoLBMApp::~RheoLBMApp() = default;
void rheo::RheoLBMApp::Run() { impl_->Run(); }
