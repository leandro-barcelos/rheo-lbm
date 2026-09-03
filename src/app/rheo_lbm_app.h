#ifndef RHEOLBM_APP_H
#define RHEOLBM_APP_H

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../renderer/renderer.h"
#include "simulation_session.h"
#include "ui_controller.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo LBM"};

class RheoLBMApp {
 public:
  RheoLBMApp() : window_(kWindowProperties) {}
  void Run();

 private:
  void Init();
  void MainLoop();
  void ProcessIntent(UiIntent const& intent);
  void UpdateDeltaTime();

  core::VulkanContext context_;
  core::Window window_;
  core::VulkanDevice vulkan_device_;
  core::VulkanSwapChain vulkan_swap_chain_;
  core::CommandPools command_pools_;
  core::FrameSync frame_sync_;
  renderer::Renderer renderer_;
  SimulationSession session_;
  UiController ui_controller_;
  double last_time_ = 0;
  double delta_time_ = 0;
  bool should_close_ = false;
};
}  // namespace app

#endif  // !RHEOLBM_APP_H
