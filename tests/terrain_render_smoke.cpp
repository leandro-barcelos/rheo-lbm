// Optional visual integration check. Opens a window, exercises resize/reload,
// and exits automatically. Run from the build directory.
#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "rheo/application/application_controller.h"
#include "rheo/assets/asset_services.h"
#include "rheo/events/input_queue.h"
#include "rheo/renderer/renderer.h"
#include "rheo/ui/user_interface.h"
class SmokeDemLoader final : public assets::IDemLoader {
 public:
  std::string path;
  std::expected<domain::SharedDem, assets::AssetError> Load(
      std::string const&) const override {
    if (!path.empty()) return assets::DemLoader{}.Load(path);
    auto dem = std::make_shared<domain::DemData>();
    dem->width = 24;
    dem->height = 18;
    dem->pixel_size_meters = {1, 1};
    dem->min_elevation = 0;
    dem->max_elevation = 13;
    for (int z = 0; z < 18; ++z)
      for (int x = 0; x < 24; ++x)
        dem->samples.push_back(
            {.coordinate = {x, z}, .elevation = 13.0F * x / 23.0F});
    return dem;
  }
};
int main(int argc, char** argv) {
  try {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    events::InputQueue inputs;
    platform::Window window(
        {.width = 1280, .height = 800, .title = "Rheo LBM terrain smoke"},
        inputs);
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
    simulation::SimulationSession session(device, pools, sync);
    SmokeDemLoader dem;
    if (argc > 1) dem.path = argv[1];
    assets::ImageLoader images;
    assets::ProjectRepository projects;
    application::ApplicationController app(dem, images, projects, session);
    renderer::Renderer renderer(window.Size());
    renderer.Init(device, swap, pools);
    ui::UserInterface ui;
    ui.Init(window, context, device, swap);
    app.Submit(application::ImportTerrain{"smoke.tif"});
    auto const start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 600 && !window.ShouldClose(); ++frame) {
      platform::Window::PollEvents();
      for (auto const& event : inputs.Drain()) {
        if (std::holds_alternative<events::WindowResizedEvent>(event)) {
          renderer.HandleInput(event);
          renderer.RequestResize();
        }
      }
      if (frame == 20)
        glfwSetWindowSize(static_cast<GLFWwindow*>(window.NativeHandle()), 1100,
                          760);
      if (frame == 40) app.Submit(application::ResetSimulation{});
      if (frame == 60) {
        auto draft = app.ViewState().simulation;
        draft.lattice.upper_elevation_margin = 3;
        app.Submit(application::UpdateSimulationDraft{draft});
      }
      if (frame == 80) app.Submit(application::NewProject{});
      if (frame == 85) app.Submit(application::ImportTerrain{"smoke.tif"});
      ui.BeginFrame();
      ui.Draw(app.ViewState(), app);
      ui.EndFrame();
      app.ProcessPendingCommands();
      app.Update(16);
      if (app.ViewState().last_error)
        throw std::runtime_error(*app.ViewState().last_error);
      renderer.RenderFrame(device, swap, sync, app.SceneState(), window, ui);
      if (frame > 85 &&
          std::chrono::steady_clock::now() - start > std::chrono::seconds(10))
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    device.LogicalDevice().waitIdle();
    ui.Shutdown();
    renderer.Shutdown();
    session.Clear();
    std::cout << "Terrain rendering, resize, reset and reimport completed\n";
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
