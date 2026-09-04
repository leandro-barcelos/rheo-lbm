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

namespace ui {

struct InputCaptureState {
  bool mouse = false;
  bool keyboard = false;
};

class UserInterface final : public renderer::IOverlayPass {
 public:
  UserInterface();
  ~UserInterface() override;

  UserInterface(UserInterface const&) = delete;
  UserInterface& operator=(UserInterface const&) = delete;

  void Init(platform::Window const& window,
            graphics::GraphicsContext const& context,
            graphics::Device const& device,
            graphics::SwapChain const& swap_chain);
  void BeginFrame();
  void Draw(application::ApplicationViewState const& state,
            application::ICommandSink& commands);
  void EndFrame();
  [[nodiscard]] InputCaptureState InputCapture() const;
  void Shutdown();

  void Render(graphics::CommandList command_list) const override;
  void OnFrameResourcesChanged(std::uint32_t image_count) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ui

#endif  // RHEO_UI_USER_INTERFACE_H
