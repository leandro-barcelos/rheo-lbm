#ifndef RHEO_EXECUTABLE_RHEO_LBM_APP_H
#define RHEO_EXECUTABLE_RHEO_LBM_APP_H

#include "rheo/application/application_controller.h"
#include "rheo/assets/asset_services.h"
#include "rheo/events/input_queue.h"
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

class RheoLBMApp {
 public:
  RheoLBMApp();
  RheoLBMApp(RheoLBMApp const&) = delete;
  RheoLBMApp& operator=(RheoLBMApp const&) = delete;
  ~RheoLBMApp() = default;

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
  assets::TerrainLoader terrain_loader_;
  assets::ImageLoader image_loader_;
  assets::ProjectRepository project_repository_;
  simulation::SimulationSession simulation_;
  application::ApplicationController application_;
  renderer::Renderer renderer_;
  ui::UserInterface ui_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
  bool alt_pressed_ = false;
};

}  // namespace rheo

#endif  // RHEO_EXECUTABLE_RHEO_LBM_APP_H
