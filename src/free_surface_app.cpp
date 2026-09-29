#include <cstdlib>

#include "model_runners.h"
#include "rheo/application/application_controller.h"
#include "rheo/application/editor_input_router.h"
#include "rheo/assets/asset_services.h"
#include "rheo/events/input_event.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/renderer.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/simulation/simulation_session.h"
#include "rheo/ui/user_interface.h"

namespace {
constexpr platform::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo LBM"};

class FreeSurfaceApp {
 public:
  FreeSurfaceApp();
  FreeSurfaceApp(FreeSurfaceApp const&) = delete;
  FreeSurfaceApp& operator=(FreeSurfaceApp const&) = delete;
  ~FreeSurfaceApp() = default;

  void Run();

 private:
  void Init();
  void MainLoop();
  void RouteInput(ui::InputCaptureState capture);
  void UpdateDeltaTime();

  runtime::ApplicationBackend backend_;
  assets::DemLoader dem_loader_;
  assets::ImageLoader image_loader_;
  assets::ProjectRepository project_repository_;
  simulation::FreeSurfaceSession simulation_;
  application::ApplicationController application_;
  renderer::Renderer renderer_;
  ui::UserInterface ui_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
  application::EditorInputRouter editor_input_;
};

FreeSurfaceApp::FreeSurfaceApp()
    : backend_(runtime::ApplicationBackend::Config{.window_properties =
                                                       kWindowProperties}),
      simulation_(backend_.Device(), backend_.CommandPools(),
                  backend_.FrameSync()),
      application_(dem_loader_, image_loader_, project_repository_,
                   simulation_),
      renderer_(backend_.Window().Size()) {}

void FreeSurfaceApp::Run() {
  Init();
  MainLoop();
  backend_.Device().LogicalDevice().waitIdle();
  ui_.Shutdown();
  renderer_.Shutdown();
}

void FreeSurfaceApp::Init() {
  backend_.Init();
  renderer_.Init(backend_.Device(), backend_.SwapChain(),
                 backend_.CommandPools());
  ui_.Init(backend_.Window(), backend_.GraphicsContext(), backend_.Device(),
           backend_.SwapChain());
  last_time_ = platform::Window::TimeSeconds();
}

void FreeSurfaceApp::MainLoop() {
  const char* smoke_frames = std::getenv("RHEO_SMOKE_FRAMES");
  int frame_limit = smoke_frames ? std::atoi(smoke_frames) : 0;
  int frame = 0;
  while (!application_.ShouldQuit() && !backend_.Window().ShouldClose() &&
         (!frame_limit || frame < frame_limit)) {
    UpdateDeltaTime();
    platform::Window::PollEvents();

    ui_.BeginFrame();
    ui_.Draw(application_.ViewState(), application_);
    ui_.EndFrame();
    application_.ProcessPendingCommands();
    renderer_.PrepareCamera(application_.SceneState(),
                            backend_.Window().Size());
    RouteInput(ui_.InputCapture());

    application_.ProcessPendingCommands();
    if (application_.ShouldQuit()) {
      break;
    }
    application_.Update(delta_time_);
    renderer_.RenderFrame(backend_.Device(), backend_.SwapChain(),
                          backend_.FrameSync(), application_.SceneState(),
                          backend_.Window(), ui_);
    ++frame;
  }
}

void FreeSurfaceApp::RouteInput(ui::InputCaptureState capture) {
  auto logical = backend_.Window().LogicalSize(),
       pixels = backend_.Window().Size();
  auto events = backend_.InputQueue().Drain();
  editor_input_.Route(
      events, {capture.mouse, capture.keyboard},
      {logical.width, logical.height}, {pixels.width, pixels.height},
      application_,
      {[this](double x, double y) { return renderer_.ScreenPointToRay(x, y); },
       [this](events::InputEvent const& event) {
         renderer_.HandleInput(event);
       },
       [this] { renderer_.RequestResize(); }});
}

void FreeSurfaceApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}

}  // namespace

void rheo::RunFreeSurfaceApp() {
  FreeSurfaceApp app;
  app.Run();
}
