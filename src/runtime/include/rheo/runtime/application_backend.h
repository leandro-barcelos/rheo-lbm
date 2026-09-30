#ifndef RHEO_RUNTIME_APPLICATION_BACKEND_H
#define RHEO_RUNTIME_APPLICATION_BACKEND_H

#include "rheo/events/input_queue.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/context.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"

namespace runtime {

class ApplicationBackend {
 public:
  struct Config {
    platform::WindowProperties window_properties;
  } __attribute__((aligned(16)));

  explicit ApplicationBackend(Config const& config);

  static void PollEvents();
  [[nodiscard]] bool IsMinimized() const;
  static void WaitEvents();
  void WaitIdle();
  [[nodiscard]] bool ShouldClose() const;
  bool RecreteSwapChain();

  [[nodiscard]] events::InputQueue& InputQueue() { return input_queue_; }
  [[nodiscard]] platform::Window& Window() { return window_; }
  [[nodiscard]] graphics::GraphicsContext& GraphicsContext() {
    return context_;
  }
  [[nodiscard]] graphics::Device& Device() { return device_; }
  [[nodiscard]] graphics::SwapChain& SwapChain() { return swap_chain_; }
  [[nodiscard]] graphics::CommandPools& CommandPools() {
    return command_pools_;
  }
  [[nodiscard]] graphics::FrameSync& FrameSync() { return frame_sync_; }

 private:
  events::InputQueue input_queue_;
  platform::Window window_;

  graphics::GraphicsContext context_;
  graphics::Device device_;
  graphics::SwapChain swap_chain_;
  graphics::CommandPools command_pools_;
  graphics::FrameSync frame_sync_;
};

}  // namespace runtime

#endif  // RHEO_RUNTIME_APPLICATION_BACKEND_H
