#ifndef RHEOLBM_UI_PARAMETERS_PANEL_H
#define RHEOLBM_UI_PARAMETERS_PANEL_H

#include <cstdint>
#include <optional>
#include <string>

#include "rheo/domain/simulation_config.h"

namespace ui {

class ParametersPanel {
 public:
  ParametersPanel(ParametersPanel const&) = delete;
  ParametersPanel(ParametersPanel&&) = delete;
  ParametersPanel& operator=(ParametersPanel const&) = delete;
  ParametersPanel& operator=(ParametersPanel&&) = delete;

  ParametersPanel() = default;
  ~ParametersPanel() = default;

  struct Events {
    std::optional<std::string> uploaded_dem_texture_path;
    std::optional<std::string> uploaded_visualization_texture_path;
    std::optional<std::string> save_simulation_path;
    std::optional<std::string> load_simulation_path;
    bool new_requested = false;
    bool quit_requested = false;
  } __attribute__((aligned(128)));

  using Values = domain::SimulationSettingsDraft;

  static constexpr float kPanelWidth = 340.0F;

  [[nodiscard]] bool Draw();
  [[nodiscard]] bool AreAllRequiredDefined() const;

  [[nodiscard]] Values const& GetValues() const { return values_; }
  void SetValues(Values const& values) { values_ = values; }
  [[nodiscard]] Events const& GetEvents() const { return events_; }
  void SetEvents(Events const& events) { events_ = events; }
  void ClearEvents() { events_ = Events{}; }
  void SetDEMTexturePath(std::string const& path) { dem_texture_path_ = path; }
  void SetSimulationConfigPath(std::string const& path) {
    simulation_config_path_ = path;
  }
  void SetVisualizationTexturePath(std::string const& path) {
    visualization_texture_path_ = path;
  }

 private:
  Events events_{};
  Values values_{};
  std::string dem_texture_path_;
  std::string visualization_texture_path_;
  std::string simulation_config_path_;
  bool help_modal_opened_ = false;
  bool menu_changed_ = false;
  void MenuBar();
  bool TabBar();
  bool TerrainTab();
  bool ParametersTab();
  void SaveFileDialog();
  void LoadFileDialog();
  void DisplayFileDialogs();
  void DisplayModals();
};

}  // namespace ui

#endif  // !RHEOLBM_UI_PARAMETERS_PANEL_H
