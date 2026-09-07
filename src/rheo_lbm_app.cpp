#include "rheo_lbm_app.h"

#include <algorithm>
#include <variant>

#include "rheo/events/input_event.h"
#include "rheo/events/key_codes.h"

rheo::RheoLBMApp::RheoLBMApp()
    : window_(kWindowProperties, input_queue_),
      simulation_(device_, command_pools_, frame_sync_),
      application_(dem_loader_, image_loader_, project_repository_,
                   simulation_),
      renderer_(window_.Size()) {}

void rheo::RheoLBMApp::Run() {
  Init();
  MainLoop();
  device_.LogicalDevice().waitIdle();
  ui_.Shutdown();
  renderer_.Shutdown();
}

void rheo::RheoLBMApp::Init() {
  auto const extensions = platform::Window::RequiredGraphicsExtensions();
  context_.Init(extensions);
  context_.CreateSurface(window_);
  device_.Init(context_, *context_.Surface());
  swap_chain_.Init(device_, *context_.Surface(), window_);
  command_pools_.Init(device_);
  frame_sync_.Init(device_);
  renderer_.Init(device_, swap_chain_, command_pools_);
  ui_.Init(window_, context_, device_, swap_chain_);
  last_time_ = platform::Window::TimeSeconds();
}

void rheo::RheoLBMApp::MainLoop() {
  while (!application_.ShouldQuit() && !window_.ShouldClose()) {
    UpdateDeltaTime();
    platform::Window::PollEvents();

    ui_.BeginFrame();
    ui_.Draw(application_.ViewState(), application_);
    ui_.EndFrame();
    application_.ProcessPendingCommands();
    renderer_.PrepareCamera(application_.SceneState(), window_.Size());
    RouteInput(ui_.InputCapture());

    application_.ProcessPendingCommands();
    if (application_.ShouldQuit()) {
      break;
    }
    application_.Update(delta_time_);
    renderer_.RenderFrame(device_, swap_chain_, frame_sync_,
                          application_.SceneState(), window_, ui_);
  }
}

void rheo::RheoLBMApp::RouteInput(ui::InputCaptureState capture) {
  auto logical = window_.LogicalSize(), pixels = window_.Size();
  auto events = input_queue_.Drain();
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

void rheo::RheoLBMApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}
