#ifndef RHEO_RENDERER_RENDERER_H
#define RHEO_RENDERER_RENDERER_H

#include <memory>

#include "rheo/application/application_state.h"
#include "rheo/events/input_event.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/overlay_pass.h"

namespace renderer {

class Renderer {
 public:
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;
  explicit Renderer(platform::WindowSize initial_window_size);
  ~Renderer();

  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;

  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain,
            graphics::CommandPools const& command_pools);
  void RenderFrame(graphics::Device const& device,
                   graphics::SwapChain& swap_chain,
                   graphics::FrameSync& frame_sync,
                   application::SceneState const& scene,
                   platform::Window const& window, IOverlayPass& overlay);
  void PrepareCamera(application::SceneState const& scene,
                     platform::WindowSize size);
  domain::Ray ScreenPointToRay(double x, double y) const;
  void HandleInput(events::InputEvent const& event);
  void RequestResize();
  void Shutdown();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_RENDERER_H
