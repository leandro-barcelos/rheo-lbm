#include "rheo/ui/user_interface.h"

#include "imgui.h"
#include "imgui_layer.h"
#include "panels/free_surface_panels.h"
#include "panels/shallow_water_panels.h"

namespace ui {

class UserInterface::Impl {
 public:
  void Init(platform::Window const& window,
            graphics::GraphicsContext const& context,
            graphics::Device const& device,
            graphics::SwapChain const& swap_chain, bool persist_layout) {
    layer_.Init(window, context, device, swap_chain);
    ImGui::GetIO().IniFilename = persist_layout ? "imgui.ini" : nullptr;
  }

  void BeginFrame() { layer_.BeginFrame(); }

  void Draw(application::ApplicationViewState const& state,
            application::ICommandSink& commands) {
    free_surface_.Draw(state, commands);
  }

  void Draw(application::ShallowWaterController& controller,
            renderer::ShallowWaterRenderer& renderer) {
    shallow_water_.Draw(controller, renderer);
  }

  void EndFrame() { layer_.EndFrame(); }

  static InputCaptureState InputCapture() {
    ImGuiIO const& imgui_io = ImGui::GetIO();
    return {.mouse = imgui_io.WantCaptureMouse,
            .keyboard = imgui_io.WantCaptureKeyboard};
  }

  void Render(graphics::CommandList command_list) const {
    layer_.Render(vk::CommandBuffer(
        static_cast<VkCommandBuffer>(command_list.native_handle)));
  }
  void OnFrameResourcesChanged(std::uint32_t image_count) {
    layer_.OnFrameResourcesChanged(image_count);
  }
  void Shutdown() { layer_.Shutdown(); }

 private:
  ImGuiLayer layer_;
  FreeSurfacePanels free_surface_;
  ShallowWaterPanels shallow_water_;
};

UserInterface::UserInterface() : impl_(std::make_unique<Impl>()) {}
UserInterface::~UserInterface() = default;

void UserInterface::Init(platform::Window const& window,
                         graphics::GraphicsContext const& context,
                         graphics::Device const& device,
                         graphics::SwapChain const& swap_chain,
                         bool persist_layout) {
  impl_->Init(window, context, device, swap_chain, persist_layout);
}

void UserInterface::BeginFrame() { impl_->BeginFrame(); }

void UserInterface::Draw(application::ApplicationViewState const& state,
                         application::ICommandSink& commands) {
  impl_->Draw(state, commands);
}

void UserInterface::Draw(application::ShallowWaterController& controller,
                         renderer::ShallowWaterRenderer& renderer) {
  impl_->Draw(controller, renderer);
}

void UserInterface::EndFrame() { impl_->EndFrame(); }

InputCaptureState UserInterface::InputCapture() {
  return ui::UserInterface::Impl::InputCapture();
}

void UserInterface::Render(graphics::CommandList command_list) const {
  impl_->Render(command_list);
}
void UserInterface::OnFrameResourcesChanged(std::uint32_t image_count) {
  impl_->OnFrameResourcesChanged(image_count);
}
void UserInterface::Shutdown() { impl_->Shutdown(); }

}  // namespace ui
