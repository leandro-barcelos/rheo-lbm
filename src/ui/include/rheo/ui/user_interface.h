#ifndef RHEO_UI_USER_INTERFACE_H
#define RHEO_UI_USER_INTERFACE_H

#include <cstdint>
#include <memory>

#include "rheo/application/application_command.h"
#include "rheo/application/application_state.h"
#include "rheo/graphics/context.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/overlay_pass.h"

namespace application {
class ShallowWaterController;
}

namespace renderer {
class ShallowWaterRenderer;
}

namespace ui {

struct InputCaptureState {
  bool mouse = false;
  bool keyboard = false;
} __attribute__((aligned(2)));

class UserInterface final : public renderer::IOverlayPass {
 public:
  UserInterface();
  ~UserInterface() override;

  void Init(platform::Window const& window,
            graphics::GraphicsContext const& context,
            graphics::Device const& device,
            graphics::SwapChain const& swap_chain, bool persist_layout = true);
  void BeginFrame();
  void Draw(application::ApplicationViewState const& state,
            application::ICommandSink& commands);
  void Draw(application::ShallowWaterController& controller,
            renderer::ShallowWaterRenderer& renderer);
  void EndFrame();
  [[nodiscard]] static InputCaptureState InputCapture();
  void Shutdown();

  void Render(graphics::CommandList command_list) const override;
  void OnFrameResourcesChanged(std::uint32_t image_count);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ui

#endif  // RHEO_UI_USER_INTERFACE_H
