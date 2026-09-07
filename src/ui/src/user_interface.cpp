#include "rheo/ui/user_interface.h"

#include <algorithm>

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

    ImGui::SetNextWindowPos(ImVec2(12, 340), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Lattice editor", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::BeginDisabled(!state.terrain_loaded);
      auto brush = state.editor.settings;
      int mode = int(brush.mode);
      char const* names[] = {"Erase",      "Terrain", "Elevation",
                             "Eyedropper", "Dam",     "Water bucket"};
      bool changed = ImGui::Combo("Tool", &mode, names, 6);
      brush.mode = domain::BrushMode(mode);
      changed |= ImGui::SliderInt("Radius (cells)", &brush.radius, 1, 32);
      if (brush.mode == domain::BrushMode::kElevation) {
        changed |= ImGui::SliderInt("Elevation (cells)", &brush.elevation, 0,
                                    state.max_elevation);
        float gray = std::clamp(
            brush.elevation / std::max(state.terrain_elevation_cells, 1.0F),
            0.0F, 1.0F);
        ImGui::Text(
            "%.2f m | Gray %d",
            state.min_elevation + brush.elevation * state.meters_per_cell,
            int(gray * 255));
        ImGui::ColorButton("Elevation gray", ImVec4(gray, gray, gray, 1),
                           ImGuiColorEditFlags_NoTooltip);
      }
      if (brush.mode == domain::BrushMode::kDam) {
        changed |= ImGui::SliderInt("Half width (cells)", &brush.dam_half_width,
                                    0, 16);
        ImGui::Text("%zu points", state.editor.dam_points.size());
        ImGui::BeginDisabled(state.editor.dam_points.size() < 2);
        if (ImGui::Button("Finish"))
          commands.Submit(application::RunEditorAction{
              application::EditorAction::kFinishDam});
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Remove point"))
          commands.Submit(application::RunEditorAction{
              application::EditorAction::kRemovePoint});
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
          commands.Submit(application::RunEditorAction{
              application::EditorAction::kCancelDam});
      }
      if (changed) commands.Submit(application::SetBrush{brush});
      ImGui::BeginDisabled(!state.can_undo);
      if (ImGui::Button("Undo (Ctrl+Z)"))
        commands.Submit(
            application::RunEditorAction{application::EditorAction::kUndo});
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!state.can_redo);
      if (ImGui::Button("Redo (Ctrl+Y)"))
        commands.Submit(
            application::RunEditorAction{application::EditorAction::kRedo});
      ImGui::EndDisabled();
      ImGui::TextUnformatted(state.has_edits ? "Modified lattice"
                                             : "Original DEM");
      ImGui::EndDisabled();
      ImGui::TextUnformatted("Left: edit | Right / Middle: pan");
      ImGui::TextUnformatted("Space + Left: pan | Wheel: zoom");
    }
    ImGui::End();
    if (state.confirm_discard) ImGui::OpenPopup("Discard lattice edits?");
    if (ImGui::BeginPopupModal("Discard lattice edits?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted(
          "Changing the grid will discard edits and history.");
      if (ImGui::Button("Discard and rebuild")) {
        commands.Submit(application::ConfirmDiscard{true});
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        commands.Submit(application::ConfirmDiscard{false});
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
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
