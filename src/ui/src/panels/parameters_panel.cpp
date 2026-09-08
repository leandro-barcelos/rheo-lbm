#include "parameters_panel.h"

#include <filesystem>
#include <limits>

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace {
constexpr const char* kUploadDialogKey = "UploadElevationTextureDialog";
constexpr const char* kUploadTerrainDialogKey = "UploadTerrainTextureDialog";
constexpr const char* kSaveSimulationDialogKey = "SaveSimulationDialog";
constexpr const char* kLoadSimulationDialogKey = "LoadSimulationDialog";
constexpr const char* kHelpModalKey = "Help";
}  // namespace

bool ui::ParametersPanel::Draw() {
  ImGuiIO const& io = ImGui::GetIO();
  if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
    help_modal_opened_ = true;
    ImGui::OpenPopup(kHelpModalKey);
  }
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
    LoadFileDialog();
  }
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    SaveFileDialog();
  }

  ImGui::SetNextWindowPos(ImVec2(12.0F, 90.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(480.0F, 220.0F), ImVec2(std::numeric_limits<float>::max(),
                                     std::numeric_limits<float>::max()));

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

  bool opened = true;
  if (!ImGui::Begin("RheoLBM - Simulation Settings", &opened, window_flags)) {
    ImGui::End();
    return false;
  }

  MenuBar();

  ImGui::InputText("Loaded simulation", &simulation_config_path_,
                   ImGuiInputTextFlags_ReadOnly);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Path to the loaded simulation file");
  }

  ImGui::Spacing();

  bool changed = TabBar();
  if (menu_changed_) {
    changed = true;
    menu_changed_ = false;
  }

  ImGui::End();

  DisplayFileDialogs();
  DisplayModals();

  return changed;
}

bool ui::ParametersPanel::AreAllRequiredDefined() const {
  return domain::ValidateSimulationSettings(values_).empty();
}

void ui::ParametersPanel::MenuBar() {
  if (!ImGui::BeginMenuBar()) {
    ImGui::EndMenuBar();
    return;
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "")) {
      values_ = Values{};
      events_ = Events{};
      dem_texture_path_.clear();
      visualization_texture_path_.clear();
      simulation_config_path_.clear();
      help_modal_opened_ = false;
      menu_changed_ = true;
      events_.new_requested = true;
    }

    if (ImGui::MenuItem("Open", "CTRL+O")) {
      LoadFileDialog();
    }

    if (ImGui::MenuItem("Save", "CTRL+S")) {
      SaveFileDialog();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Select DEM")) {
      IGFD::FileDialogConfig config{};
      config.path = std::filesystem::current_path().string();
      config.filePathName = dem_texture_path_;
      config.flags = ImGuiFileDialogFlags_Modal;

      ImGuiFileDialog::Instance()->OpenDialog(
          kUploadDialogKey, "Select Digital Elevation Model",
          "GeoTiff files{.tif,.tiff},.*", config);
    }

    ImGui::Separator();

    bool debug = true;
    if (ImGui::MenuItem("Debug", "", debug)) {
      // TODO
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Quit", "ALT+F4")) {
      events_.quit_requested = true;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("Help", "F1")) {
      help_modal_opened_ = true;
      ImGui::OpenPopup(kHelpModalKey);
    }

    ImGui::EndMenu();
  }

  ImGui::EndMenuBar();
}

bool ui::ParametersPanel::TabBar() {
  bool changed = false;

  if (ImGui::BeginTabBar("TabBar")) {
    changed |= TerrainTab();
    changed |= ParametersTab();

    ImGui::EndTabBar();
  }

  return changed;
}

bool ui::ParametersPanel::TerrainTab() {
  bool changed = false;

  if (ImGui::BeginTabItem("Terrain")) {
    ImGui::BeginDisabled(locked_);
    ImGui::InputText("DEM", &dem_texture_path_, ImGuiInputTextFlags_ReadOnly);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Path to the DEM file");
    }

    ImGui::SetNextItemWidth(180.0F);
    changed |= ImGui::InputInt("Height subdivisions",
                               &values_.lattice.height_subdivisions);
    ImGui::SetNextItemWidth(180.0F);
    changed |=
        ImGui::InputFloat("Upper elevation margin (m)",
                          &values_.lattice.upper_elevation_margin, 1.0F, 10.0F);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  return changed;
}

bool ui::ParametersPanel::ParametersTab() {
  bool changed = false;

  if (!ImGui::BeginTabItem("Parameters")) {
    return changed;
  }
  ImGui::BeginDisabled(locked_);

  auto& p = values_.lbm;
  ImGui::TextUnformatted("D3Q19 free-surface LBM");
  changed |=
      ImGui::InputFloat("Steps per second", &p.steps_per_second, 1, 10, "%.1f");
  changed |= ImGui::InputFloat("Initial density", &p.initial_density, .01F, .1F,
                               "%.4f");
  changed |= ImGui::SliderFloat("Omega", &p.omega, 0.01F, 1.99F, "%.4f");
  changed |= ImGui::InputFloat("Atmospheric density", &p.atmospheric_density,
                               .01F, .1F, "%.4f");
  changed |=
      ImGui::InputFloat("Maximum velocity", &p.max_velocity, .01F, .1F, "%.4f");
  changed |=
      ImGui::InputFloat("Fill offset", &p.fill_offset, .001F, .01F, "%.4f");
  changed |=
      ImGui::SliderFloat("Lonely threshold", &p.lonely_threshold, 0, 1, "%.3f");
  changed |= ImGui::InputFloat3("Gravity", &p.gravity.x, "%.5f");

  ImGui::EndDisabled();
  ImGui::EndTabItem();

  return changed;
}

void ui::ParametersPanel::SaveFileDialog() {
  IGFD::FileDialogConfig config{};
  config.path = std::filesystem::current_path().string();
  config.filePathName = simulation_config_path_;
  config.flags =
      ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;

  ImGuiFileDialog::Instance()->OpenDialog(kSaveSimulationDialogKey,
                                          "Save Simulation Settings",
                                          "YAML files{.yaml,.yml},.*", config);
}

void ui::ParametersPanel::LoadFileDialog() {
  IGFD::FileDialogConfig config{};
  config.path = std::filesystem::current_path().string();
  config.filePathName = simulation_config_path_;
  config.flags = ImGuiFileDialogFlags_Modal;

  ImGuiFileDialog::Instance()->OpenDialog(kLoadSimulationDialogKey,
                                          "Open Simulation Settings",
                                          "YAML files{.yaml,.yml},.*", config);
}

void ui::ParametersPanel::DisplayFileDialogs() {
  if (ImGuiFileDialog::Instance()->Display(
          kUploadDialogKey, ImGuiWindowFlags_NoCollapse, ImVec2(600.0F, 450.0F),
          ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.uploaded_dem_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kUploadTerrainDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.uploaded_visualization_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kSaveSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.save_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kLoadSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.load_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void ui::ParametersPanel::DisplayModals() {
  if (ImGui ::BeginPopupModal(kHelpModalKey, &help_modal_opened_)) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Keybinds");

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1});
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::TextColored(
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
        "Right / Middle / Space + Left = Camera pan");

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1});
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::TextColored(
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
        "Scroll = Zoom | Left = Edit | Ctrl+Z / Ctrl+Y = Undo / Redo");

    ImGui::EndPopup();
  }
}
