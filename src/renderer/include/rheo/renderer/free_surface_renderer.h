#ifndef RHEO_RENDERER_FREE_SURFACE_RENDERER_H
#define RHEO_RENDERER_FREE_SURFACE_RENDERER_H

#include <memory>

#include "rheo/application/application_state.h"
#include "rheo/events/input_event.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/scene_pass.h"

namespace renderer {

class FreeSurfaceRenderer {
 public:
  explicit FreeSurfaceRenderer(platform::WindowSize initial_window_size);
  ~FreeSurfaceRenderer();
  FreeSurfaceRenderer(FreeSurfaceRenderer const&) = delete;
  FreeSurfaceRenderer& operator=(FreeSurfaceRenderer const&) = delete;
  FreeSurfaceRenderer(FreeSurfaceRenderer&&) = delete;
  FreeSurfaceRenderer& operator=(FreeSurfaceRenderer&&) = delete;

  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain);
  void OnSwapChainRecreated(graphics::Device const& device,
                            graphics::SwapChain const& swap_chain);
  void PrepareCamera(application::SceneState const& scene,
                     platform::WindowSize size);
  [[nodiscard]] domain::Ray ScreenPointToRay(double xpos, double ypos) const;
  void HandleInput(events::InputEvent const& event);
  void Shutdown();

  // Borrows the renderer and scene for one synchronous RenderFrame call.
  class Pass final : public IScenePass {
   public:
    Pass(FreeSurfaceRenderer& renderer, application::SceneState const& scene);
    std::uint64_t ReadySignal() const override;
    std::optional<vk::RenderingAttachmentInfo> Prepare(
        vk::raii::CommandBuffer const& command,
        graphics::SwapChain const& swap_chain) override;
    void Render(vk::raii::CommandBuffer const& command,
                graphics::SwapChain const& swap_chain) override;

   private:
    FreeSurfaceRenderer& renderer_;
    application::SceneState const& scene_;
  };

  [[nodiscard]] Pass MakePass(application::SceneState const& scene);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_FREE_SURFACE_RENDERER_H
