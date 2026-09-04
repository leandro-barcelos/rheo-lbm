#include "rheo/ui/user_interface.h"

#include "imgui.h"
#include "imgui_layer.h"
#include "panels/control_panel.h"
#include "panels/parameters_panel.h"

namespace ui {

class UserInterface::Impl {
 public:
  void Init(platform::Window const& window,
            graphics::GraphicsContext const& context,
            graphics::Device const& device,
            graphics::SwapChain const& swap_chain) {
    layer_.Init(window, context, device, swap_chain);
  }

  void BeginFrame() const { layer_.BeginFrame(); }

  void Draw(application::ApplicationViewState const& state,
            application::ICommandSink& commands) {
    parameters_.SetValues(state.simulation);
    parameters_.SetDEMTexturePath(state.terrain_path);
    parameters_.SetVisualizationTexturePath(state.terrain_texture_path);
    parameters_.SetSimulationConfigPath(state.project_path);

    bool const draft_changed = parameters_.Draw();
    auto const events = parameters_.GetEvents();

    if (events.new_requested) {
      commands.Submit(application::NewProject{});
    } else if (draft_changed) {
      commands.Submit(
          application::UpdateSimulationDraft{parameters_.GetValues()});
    }
    if (events.uploaded_dem_texture_path) {
      commands.Submit(
          application::ImportTerrain{*events.uploaded_dem_texture_path});
    }
    if (events.uploaded_visualization_texture_path) {
      commands.Submit(application::SetTerrainTexture{
          *events.uploaded_visualization_texture_path});
    }
    if (events.save_simulation_path) {
      commands.Submit(application::SaveProject{*events.save_simulation_path});
    }
    if (events.load_simulation_path) {
      commands.Submit(application::LoadProject{*events.load_simulation_path});
    }
    if (events.quit_requested) {
      commands.Submit(application::RequestQuit{});
    }

    auto const controls = control_.Draw(state.simulation_running,
                                        state.can_play, state.terrain_loaded);
    if (controls.play_pressed) {
      commands.Submit(application::PlaySimulation{});
    } else if (controls.pause_pressed) {
      commands.Submit(application::PauseSimulation{});
    } else if (controls.reset_pressed) {
      commands.Submit(application::ResetSimulation{});
    }

    if (state.last_error) {
      ImGui::SetNextWindowPos(ImVec2(12.0F, 640.0F), ImGuiCond_FirstUseEver);
      if (ImGui::Begin("Status", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0F, 0.35F, 0.35F, 1.0F), "%s",
                           state.last_error->c_str());
      }
      ImGui::End();
    }
    if (!state.validation_errors.empty()) {
      ImGui::SetNextWindowPos(ImVec2(12.0F, 520.0F), ImGuiCond_FirstUseEver);
      if (ImGui::Begin("Validation", nullptr,
                       ImGuiWindowFlags_AlwaysAutoResize)) {
        for (auto const& error : state.validation_errors) {
          ImGui::BulletText("%s", error.c_str());
        }
      }
      ImGui::End();
    }
    parameters_.ClearEvents();
  }

  void EndFrame() const { layer_.EndFrame(); }

  [[nodiscard]] InputCaptureState InputCapture() const {
    ImGuiIO const& io = ImGui::GetIO();
    return {.mouse = io.WantCaptureMouse, .keyboard = io.WantCaptureKeyboard};
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
  ParametersPanel parameters_;
  ImGuiLayer layer_;
  ControlPanel control_;
};

UserInterface::UserInterface() : impl_(std::make_unique<Impl>()) {}
UserInterface::~UserInterface() = default;

void UserInterface::Init(platform::Window const& window,
                         graphics::GraphicsContext const& context,
                         graphics::Device const& device,
                         graphics::SwapChain const& swap_chain) {
  impl_->Init(window, context, device, swap_chain);
}
void UserInterface::BeginFrame() { impl_->BeginFrame(); }
void UserInterface::Draw(application::ApplicationViewState const& state,
                         application::ICommandSink& commands) {
  impl_->Draw(state, commands);
}
void UserInterface::EndFrame() { impl_->EndFrame(); }
InputCaptureState UserInterface::InputCapture() const {
  return impl_->InputCapture();
}
void UserInterface::Shutdown() { impl_->Shutdown(); }
void UserInterface::Render(graphics::CommandList command_list) const {
  impl_->Render(command_list);
}
void UserInterface::OnFrameResourcesChanged(std::uint32_t image_count) {
  impl_->OnFrameResourcesChanged(image_count);
}

}  // namespace ui
