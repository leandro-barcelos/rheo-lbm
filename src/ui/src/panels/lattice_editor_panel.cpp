#include "lattice_editor_panel.h"

#include <algorithm>
#include <array>

#include "imgui.h"
#include "rheo/domain/lattice_editing.h"

namespace ui {

bool LatticeEditorPanel::Draw(application::ApplicationViewState const& state,
                              application::ICommandSink& commands) {
  bool changed = false;
  ImGui::SetNextWindowPos(ImVec2(12, 340), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Lattice editor", nullptr,
                   ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginDisabled(!state.can_edit);
    auto brush = state.editor.settings;

    int mode = int(brush.mode);
    std::array names = {"Erase",      "Terrain", "Elevation",
                        "Eyedropper", "Dam",     "Water bucket"};
    changed = ImGui::Combo("Tool", &mode, names.data(), 6);
    brush.mode = domain::BrushMode(mode);
    changed |= ImGui::SliderInt("Radius (cells)", &brush.radius, 1, 32);

    if (brush.mode == domain::BrushMode::kElevation) {
      changed |= ImGui::SliderInt("Elevation (cells)", &brush.elevation, 0,
                                  state.max_elevation);
      float gray = std::clamp(static_cast<float>(brush.elevation) /
                                  std::max(state.terrain_elevation_cells, 1.0F),
                              0.0F, 1.0F);
      ImGui::Text("%.2f m | Gray %d",
                  state.min_elevation + (state.meters_per_cell *
                                         static_cast<float>(brush.elevation)),
                  int(gray * 255));
      ImGui::ColorButton("Elevation gray", ImVec4(gray, gray, gray, 1),
                         ImGuiColorEditFlags_NoTooltip);
    }

    if (brush.mode == domain::BrushMode::kDam) {
      changed |=
          ImGui::SliderInt("Half width (cells)", &brush.dam_half_width, 0, 16);
      ImGui::Text("%zu points", state.editor.dam_points.size());
      ImGui::BeginDisabled(state.editor.dam_points.size() < 2);
      if (ImGui::Button("Finish")) {
        commands.Submit(application::RunEditorAction{
            application::EditorAction::kFinishDam});
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Remove point")) {
        commands.Submit(application::RunEditorAction{
            application::EditorAction::kRemovePoint});
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        commands.Submit(application::RunEditorAction{
            application::EditorAction::kCancelDam});
      }
    }

    if (changed) {
      commands.Submit(application::SetBrush{brush});
    }

    ImGui::BeginDisabled(!state.can_undo);
    if (ImGui::Button("Undo (Ctrl+Z)")) {
      commands.Submit(
          application::RunEditorAction{application::EditorAction::kUndo});
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.can_redo);
    if (ImGui::Button("Redo (Ctrl+Y)")) {
      commands.Submit(
          application::RunEditorAction{application::EditorAction::kRedo});
    }

    ImGui::EndDisabled();
    ImGui::TextUnformatted(state.has_edits ? "Modified lattice"
                                           : "Original DEM");
    ImGui::EndDisabled();
    ImGui::TextUnformatted("Left: edit | Right / Middle: pan");
    ImGui::TextUnformatted("Space + Left: pan | Wheel: zoom");
  }
  ImGui::End();

  DiscardEditsPopup(state, commands);

  return changed;
}

void LatticeEditorPanel::DiscardEditsPopup(
    application::ApplicationViewState const& state,
    application::ICommandSink& commands) {
  if (state.confirm_discard) {
    ImGui::OpenPopup("Discard lattice edits?");
  }

  if (ImGui::BeginPopupModal("Discard lattice edits?", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Changing the grid will discard edits and history.");
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
}

}  // namespace ui
