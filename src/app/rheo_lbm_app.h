#ifndef RHEOLBM_APP_H
#define RHEOLBM_APP_H

#include "rheo-lbm/src/core/command_pool.h"
#include "rheo-lbm/src/core/frame_sync.h"
#include "rheo-lbm/src/core/vulkan_context.h"
#include "rheo-lbm/src/core/vulkan_device.h"
#include "rheo-lbm/src/core/vulkan_swap_chain.h"
#include "rheo-lbm/src/core/window.h"
#include "rheo-lbm/src/events/keyboard_event.h"
#include "rheo-lbm/src/events/window_event.h"
#include "rheo-lbm/src/renderer/renderer.h"
#include "simulation_session.h"
#include "ui_controller.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo LBM"};

class RheoLBMApp {
 public:
  RheoLBMApp();
  RheoLBMApp(const RheoLBMApp&) = delete;
  RheoLBMApp(RheoLBMApp&&) = delete;
  RheoLBMApp& operator=(const RheoLBMApp&) = delete;
  RheoLBMApp& operator=(RheoLBMApp&&) = delete;
  ~RheoLBMApp();

  void Run();

 private:
  void Init();
  void MainLoop();
  void ProcessIntent(UiIntent const& intent);
  void UpdateDeltaTime();
  void OnKeyPressedEvent(events::KeyPressedEvent const& event);
  void OnKeyReleasedEvent(events::KeyReleasedEvent const& event);
  void OnWindowResizedEvent(events::WindowResizedEvent const& event);
  void RecreateSwapChainAndNotifyRenderer();

  events::EventHandler<events::KeyPressedEvent> key_pressed_event_handler_;
  events::EventHandler<events::KeyReleasedEvent> key_released_event_handler_;
  events::EventHandler<events::WindowResizedEvent>
      window_resized_event_handler_;

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
  bool swap_chain_recreate_pending_ = false;
  bool alt_pressed_ = false;
};

}  // namespace app

#endif  // !RHEOLBM_APP_H
