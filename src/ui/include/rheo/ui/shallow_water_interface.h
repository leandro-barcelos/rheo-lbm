#pragma once
#include <memory>

#include "rheo/application/shallow_water_controller.h"
#include "rheo/graphics/context.h"
#include "rheo/renderer/shallow_water_renderer.h"
namespace ui {
class ShallowWaterInterface final : public renderer::IOverlayPass {
 public:
  ShallowWaterInterface();
  ~ShallowWaterInterface() override;
  void Init(const platform::Window&, const graphics::GraphicsContext&,
            const graphics::Device&, const graphics::SwapChain&);
  void BeginFrame();
  void Draw(application::ShallowWaterController&,
            renderer::ShallowWaterRenderer&);
  void EndFrame();
  void Shutdown();
  void Render(graphics::CommandList) const override;
  void OnFrameResourcesChanged(std::uint32_t) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace ui
