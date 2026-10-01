#include "free_surface_panels.h"

#include "imgui.h"
#include "panels/lattice_editor_panel.h"

namespace ui {

void FreeSurfacePanels::Draw(application::ApplicationViewState const& state,
                             application::ICommandSink& commands) {
  parameters_.SetValues(state.simulation);
  parameters_.SetDEMTexturePath(state.terrain_path);
  parameters_.SetVisualizationTexturePath(state.terrain_texture_path);
  parameters_.SetSimulationConfigPath(state.project_path);
  parameters_.SetLocked(!state.can_edit);

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

  auto const controls = control_.Draw(
      state.simulation_running, state.simulation_paused, state.can_play,
      state.terrain_loaded, state.can_remove_dam, state.physical_step_count);

  if (controls.play_pressed) {
    commands.Submit(application::PlaySimulation{});
  } else if (controls.pause_pressed) {
    commands.Submit(application::PauseSimulation{});
  } else if (controls.reset_pressed) {
    commands.Submit(application::ResetSimulation{});
  } else if (controls.remove_dam_pressed) {
    commands.Submit(application::RemoveDam{});
  }

  ui::LatticeEditorPanel::Draw(state, commands);

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

}  // namespace ui
