#ifndef RHEO_APPLICATION_APPLICATION_STATE_H
#define RHEO_APPLICATION_APPLICATION_STATE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "rheo/domain/dem_data.h"
#include "rheo/domain/image_data.h"
#include "rheo/domain/simulation_config.h"
#include "rheo/simulation/simulation_types.h"

namespace application {

struct ApplicationViewState {
  domain::SimulationSettingsDraft simulation;
  std::string terrain_path;
  std::string terrain_texture_path;
  std::string project_path;
  std::optional<std::string> last_error;
  std::vector<std::string> validation_errors;
  domain::BrushPreview editor;
  bool has_edits = false, can_undo = false, can_redo = false,
       confirm_discard = false;
  int max_elevation = 0;
  float meters_per_cell = 0, min_elevation = 0, terrain_elevation_cells = 1;
  bool can_play = false;
  bool simulation_running = false;
  bool simulation_paused = false;
  bool can_edit = false;
  bool can_remove_dam = false;
  bool terrain_loaded = false;
  std::uint64_t physical_step_count = 0;
};

struct SceneState {
  domain::SharedDem dem;
  domain::SharedImage terrain_texture;
  std::optional<simulation::LatticeRenderSnapshot> lattice;
  domain::BrushPreview preview;
  std::uint64_t revision = 0;
} __attribute__((aligned(128)));

}  // namespace application

#endif  // RHEO_APPLICATION_APPLICATION_STATE_H
