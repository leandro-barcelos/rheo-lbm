#include "rheo_lbm_app.h"

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
  for (auto const& event : input_queue_.Drain()) {
    if (auto const* resized = std::get_if<events::WindowResizedEvent>(&event)) {
      if (resized->width > 0 && resized->height > 0) {
        renderer_.RequestResize();
        renderer_.HandleInput(event);
      }
      continue;
    }

    if (auto const* pressed = std::get_if<events::KeyPressedEvent>(&event)) {
      if (pressed->key == events::kLeftAlt ||
          pressed->key == events::kRightAlt) {
        alt_pressed_ = true;
      } else if (pressed->key == events::kF4 && alt_pressed_) {
        application_.Submit(application::RequestQuit{});
      }
    } else if (auto const* released =
                   std::get_if<events::KeyReleasedEvent>(&event)) {
      if (released->key == events::kLeftAlt ||
          released->key == events::kRightAlt) {
        alt_pressed_ = false;
      }
    }

    if ((events::IsKeyboardEvent(event) && capture.keyboard) ||
        (events::IsMouseEvent(event) && capture.mouse)) {
      continue;
    }
    renderer_.HandleInput(event);
  }
}

void rheo::RheoLBMApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}
