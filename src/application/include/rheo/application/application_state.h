#ifndef RHEO_APPLICATION_APPLICATION_STATE_H
#define RHEO_APPLICATION_APPLICATION_STATE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "rheo/domain/image_data.h"
#include "rheo/domain/simulation_config.h"
#include "rheo/domain/terrain_data.h"
#include "rheo/simulation/fluid_types.h"

namespace application {

struct ApplicationViewState {
  domain::SimulationSettingsDraft simulation;
  std::string terrain_path;
  std::string terrain_texture_path;
  std::string project_path;
  std::optional<std::string> last_error;
  std::vector<std::string> validation_errors;
  bool can_play = false;
  bool simulation_running = false;
  bool terrain_loaded = false;
};

struct SceneState {
  domain::SharedTerrain terrain;
  domain::SharedImage terrain_texture;
  std::optional<simulation::FluidRenderSnapshot> fluid;
  std::uint64_t revision = 0;
};

}  // namespace application

#endif  // RHEO_APPLICATION_APPLICATION_STATE_H
