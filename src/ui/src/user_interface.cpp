#include "rheo/ui/user_interface.h"

#include "imgui.h"

namespace ui {

void UserInterface::Init(platform::Window const& window,
                         graphics::GraphicsContext const& context,
                         graphics::Device const& device,
                         graphics::SwapChain const& swap_chain,
                         bool persist_layout) {
  layer_.Init(window, context, device, swap_chain);
  ImGui::GetIO().IniFilename = persist_layout ? "imgui.ini" : nullptr;
}

void UserInterface::BeginFrame() { layer_.BeginFrame(); }

void UserInterface::Draw(application::ApplicationViewState const& state,
                         application::ICommandSink& commands) {
  free_surface_.Draw(state, commands);
}

void UserInterface::Draw(application::ShallowWaterController& controller,
                         renderer::ShallowWaterRenderer& renderer) {
  shallow_water_.Draw(controller, renderer);
}

void UserInterface::EndFrame() { layer_.EndFrame(); }

InputCaptureState UserInterface::InputCapture() {
  ImGuiIO const& imgui_io = ImGui::GetIO();
  return {.mouse = imgui_io.WantCaptureMouse, .keyboard = imgui_io.WantCaptureKeyboard};
}

void UserInterface::Render(graphics::CommandList command_list) const {
  layer_.Render(vk::CommandBuffer(
      static_cast<VkCommandBuffer>(command_list.native_handle)));
}
void UserInterface::OnFrameResourcesChanged(std::uint32_t image_count) {
  layer_.OnFrameResourcesChanged(image_count);
}
void UserInterface::Shutdown() { layer_.Shutdown(); }

}  // namespace ui
