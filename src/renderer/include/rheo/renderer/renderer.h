#ifndef RHEO_RENDERER_RENDERER_H
#define RHEO_RENDERER_RENDERER_H

#include <memory>

#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/renderer/overlay_pass.h"
#include "rheo/renderer/render_result.h"
#include "rheo/renderer/scene_pass.h"

namespace renderer {

class Renderer {
 public:
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;
  Renderer();
  ~Renderer();

  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;

  void Init(graphics::Device const& device,
            graphics::CommandPools const& command_pools);
  [[nodiscard]] RenderResult RenderFrame(graphics::Device const& device,
                                         graphics::SwapChain& swap_chain,
                                         graphics::FrameSync& frame_sync,
                                         IOverlayPass const& overlay,
                                         IScenePass* scene = nullptr);
  void Shutdown();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_RENDERER_H
