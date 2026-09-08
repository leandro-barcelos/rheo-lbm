#include "control_panel.h"

#include "IconsFontAwesome6.h"
#include "imgui.h"

ui::ControlPanel::Events ui::ControlPanel::Draw(bool simulation_running,
                                                bool simulation_paused,
                                                bool can_play, bool can_restore,
                                                bool can_remove_dam,
                                                std::uint64_t physical_steps) {
  Events events{};

  ImGui::SetNextWindowPos(ImVec2(12.0F, 36.0F), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;

  if (ImGui::Begin("Controls", nullptr, window_flags)) {
    ImGui::Text("Simulation");

    ImGui::BeginDisabled(simulation_running || !can_play);
    if (ImGui::Button(ICON_FA_PLAY)) {
      events.play_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip(simulation_paused ? "Resume" : "Play");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!simulation_running);
    if (ImGui::Button(ICON_FA_PAUSE)) {
      events.pause_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip("Pause");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!can_restore);
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT)) {
      events.reset_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip("Restore terrain");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_remove_dam);
    if (ImGui::Button(ICON_FA_TRASH_CAN " Remove dam")) {
      events.remove_dam_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip("Remove every dam cell while the simulation runs");
    }
    ImGui::Text("Physical steps: %llu",
                static_cast<unsigned long long>(physical_steps));
  }

  ImGui::End();

  return events;
}

void ui::ControlPanel::DrawDamControls(bool simulation_running,
                                       bool can_edit_dam,
                                       ui::ControlPanel::Events& events) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextUnformatted("Dam");

  ImGui::BeginDisabled(simulation_running || !can_edit_dam);

  bool const draw_selected = dam_mode_ == ui::ControlPanel::DamMode::kDraw;
  if (ImGui::Selectable(ICON_FA_PENCIL " Draw", draw_selected)) {
    dam_mode_ = draw_selected ? ui::ControlPanel::DamMode::kNone
                              : ui::ControlPanel::DamMode::kDraw;
    events.draw_dam_pressed = !draw_selected;
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
    if (can_edit_dam) {
      ImGui::SetTooltip("Enter \"Draw dam wall\" mode");
    } else {
      ImGui::SetTooltip("Import a DEM first");
    }
  }

  bool const fill_selected = dam_mode_ == ui::ControlPanel::DamMode::kFill;
  if (ImGui::Selectable(ICON_FA_BUCKET " Fill", fill_selected)) {
    dam_mode_ = fill_selected ? ui::ControlPanel::DamMode::kNone
                              : ui::ControlPanel::DamMode::kFill;
    events.fill_dam_pressed = !fill_selected;
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
    if (can_edit_dam) {
      ImGui::SetTooltip("Enter \"Fill dam\" mode");
    } else {
      ImGui::SetTooltip("Import a DEM first");
    }
  }

  ImGui::EndDisabled();
}
