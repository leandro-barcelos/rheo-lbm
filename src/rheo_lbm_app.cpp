#include "rheo_lbm_app.h"

#include <algorithm>
#include <variant>

#include "rheo/application/application_controller.h"
#include "rheo/application/editor_input_router.h"
#include "rheo/assets/asset_services.h"
#include "rheo/events/input_event.h"
#include "rheo/events/input_queue.h"
#include "rheo/events/key_codes.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/context.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/renderer.h"
#include "rheo/simulation/simulation_session.h"
#include "rheo/ui/user_interface.h"

namespace rheo {
constexpr platform::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo LBM"};

class RheoLBMApp::Impl {
 public:
  Impl();
  Impl(Impl const&) = delete;
  Impl& operator=(Impl const&) = delete;
  ~Impl() = default;

  void Run();

 private:
  void Init();
  void MainLoop();
  void RouteInput(ui::InputCaptureState capture);
  void UpdateDeltaTime();

  events::InputQueue input_queue_;
  platform::Window window_;
  graphics::GraphicsContext context_;
  graphics::Device device_;
  graphics::SwapChain swap_chain_;
  graphics::CommandPools command_pools_;
  graphics::FrameSync frame_sync_;
  assets::DemLoader dem_loader_;
  assets::ImageLoader image_loader_;
  assets::ProjectRepository project_repository_;
  simulation::SimulationSession simulation_;
  application::ApplicationController application_;
  renderer::Renderer renderer_;
  ui::UserInterface ui_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
  application::EditorInputRouter editor_input_;
};

}  // namespace rheo

rheo::RheoLBMApp::Impl::Impl()
    : window_(kWindowProperties, input_queue_),
      simulation_(device_, command_pools_, frame_sync_),
      application_(dem_loader_, image_loader_, project_repository_,
                   simulation_),
      renderer_(window_.Size()) {}

void rheo::RheoLBMApp::Impl::Run() {
  Init();
  MainLoop();
  device_.LogicalDevice().waitIdle();
  ui_.Shutdown();
  renderer_.Shutdown();
}

void rheo::RheoLBMApp::Impl::Init() {
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

void rheo::RheoLBMApp::Impl::MainLoop() {
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

void rheo::RheoLBMApp::Impl::RouteInput(ui::InputCaptureState capture) {
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

void rheo::RheoLBMApp::Impl::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}

rheo::RheoLBMApp::RheoLBMApp() : impl_(std::make_unique<Impl>()) {}
rheo::RheoLBMApp::~RheoLBMApp() = default;
void rheo::RheoLBMApp::Run() { impl_->Run(); }
