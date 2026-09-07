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
    if (!path.empty() && path != "basin") return assets::DemLoader{}.Load(path);
    auto dem = std::make_shared<domain::DemData>();
    dem->width = 24;
    dem->height = 18;
    dem->pixel_size_meters = {1, 1};
    dem->min_elevation = 0;
    dem->max_elevation = 13;
    for (int z = 0; z < 18; ++z)
      for (int x = 0; x < 24; ++x)
        dem->samples.push_back(
            {.coordinate = {x, z},
             .elevation =
                 path == "basin"
                     ? ((x < 3 || x > 20 || z < 3 || z > 14) ? 13.0F : 0.0F)
                     : 13.0F * x / 23.0F});
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
    auto command = [&](application::ApplicationCommand c) {
      app.Submit(std::move(c));
      app.ProcessPendingCommands();
    };
    auto pointer = [&](float x, float z, bool press = false,
                       bool release = false) {
      auto d = *session.Editing().definition;
      float scale = std::max({d.width, d.height, d.depth});
      command(application::BrushPointer{
          domain::Ray{{(x + .5F - d.width * .5F) / scale, 2,
                       (z + .5F - d.depth * .5F) / scale},
                      {0, -1, 0}},
          {x, z},
          press,
          release});
    };
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
      if (frame == 95) {
        auto d = *session.Editing().definition;
        command(application::SetBrush{{.mode = domain::BrushMode::kElevation,
                                       .radius = 2,
                                       .elevation = int(d.height) - 2}});
        pointer(d.width * .4F, d.depth * .5F, true);
        pointer(d.width * .45F, d.depth * .5F);
        pointer(d.width * .45F, d.depth * .5F, false, true);
      }
      if (frame == 100)
        command(application::RunEditorAction{application::EditorAction::kUndo});
      if (frame == 105)
        command(application::RunEditorAction{application::EditorAction::kRedo});
      if (frame == 110) {
        auto d = *session.Editing().definition;
        command(application::SetBrush{{.mode = domain::BrushMode::kDam}});
        pointer(d.width * .4F, d.depth * .4F, true);
        pointer(d.width * .7F, d.depth * .7F, true);
      }
      if (frame == 115)
        command(application::RunEditorAction{
            application::EditorAction::kFinishDam});
      if (frame == 120) {
        auto d = *session.Editing().definition;
        command(application::SetBrush{{.mode = domain::BrushMode::kWater}});
        pointer(d.width * .7F, d.depth * .5F, true);
        if (dem.path == "basin" &&
            std::ranges::find(session.Editing().types,
                              domain::Type(domain::CellType::kFluid)) ==
                session.Editing().types.end())
          throw std::runtime_error(
              "Closed basin did not produce visible fluid");
      }
      if (frame == 125)
        command(application::SaveProject{"/tmp/rheo-editor-smoke.yaml"});
      if (frame == 130)
        command(application::LoadProject{"/tmp/rheo-editor-smoke.yaml"});
      if (frame >= 135) {
        auto signal = session.Update(0)->ready_signal;
        auto d = *session.Editing().definition;
        if (frame == 135)
          command(application::SetBrush{{.mode = domain::BrushMode::kElevation,
                                         .radius = 2,
                                         .elevation = int(d.height) - 2}});
        pointer(d.width * .5F, d.depth * .5F);
        if (session.Update(0)->ready_signal != signal)
          throw std::runtime_error("Preview dispatched a lattice edit");
      }
      app.Update(16);
      if (app.ViewState().last_error)
        throw std::runtime_error(*app.ViewState().last_error);
      renderer.RenderFrame(device, swap, sync, app.SceneState(), window, ui);
      if (frame > 135 &&
          std::chrono::steady_clock::now() - start > std::chrono::seconds(10))
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    device.LogicalDevice().waitIdle();
    ui.Shutdown();
    renderer.Shutdown();
    session.Clear();
    std::cout << "Terrain rendering, editing, previews, undo/redo, save/load "
                 "and resize completed\n";
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
