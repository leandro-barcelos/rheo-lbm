#ifndef RHEO_DOMAIN_SIMULATION_CONFIG_H
#define RHEO_DOMAIN_SIMULATION_CONFIG_H

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rheo/domain/terrain_data.h"

namespace domain {

struct SimulationSettingsDraft {
  std::optional<float> total_fluid_volume;
  std::optional<float> initial_particle_spacing;
  std::optional<float> dem_resolution;
  std::optional<std::uint32_t> voxel_max_particles;
  std::optional<float> viscosity;
  std::optional<float> rest_density;
  std::optional<float> gas_constant;
  std::optional<float> coefficient_of_restitution;
  std::optional<float> friction;
  std::optional<float> yield_stress;
};

struct SimulationConfig {
  std::uint32_t voxel_max_particles = 0;
  float rest_density = 0.0F;
  float total_fluid_volume = 0.0F;
  float viscosity = 0.0F;
  float gas_constant = 0.0F;
  float coefficient_of_restitution = 0.0F;
  SharedTerrain terrain;
  float friction = 0.0F;
  float yield_stress = 0.0F;
  float initial_particle_spacing = 0.0F;
};

[[nodiscard]] inline std::vector<std::string> ValidateSimulationSettings(
    SimulationSettingsDraft const& draft) {
  std::vector<std::string> errors;
  auto require_positive = [&errors](std::optional<float> value,
                                    char const* name) {
    if (!value || !std::isfinite(*value) || *value <= 0.0F) {
      errors.emplace_back(std::string(name) + " must be greater than zero");
    }
  };
  auto require_non_negative = [&errors](std::optional<float> value,
                                        char const* name) {
    if (!value || !std::isfinite(*value) || *value < 0.0F) {
      errors.emplace_back(std::string(name) + " cannot be negative");
    }
  };

  require_positive(draft.total_fluid_volume, "Total fluid volume");
  require_positive(draft.initial_particle_spacing, "Initial particle spacing");
  if (draft.dem_resolution && (!std::isfinite(*draft.dem_resolution) ||
                               *draft.dem_resolution <= 0.0F)) {
    errors.emplace_back("DEM resolution must be greater than zero");
  }
  if (!draft.voxel_max_particles || *draft.voxel_max_particles == 0) {
    errors.emplace_back(
        "Maximum particles per voxel must be greater than zero");
  }
  require_positive(draft.viscosity, "Viscosity");
  require_positive(draft.rest_density, "Rest density");
  require_positive(draft.gas_constant, "Gas constant");
  if (!draft.coefficient_of_restitution ||
      !std::isfinite(*draft.coefficient_of_restitution) ||
      *draft.coefficient_of_restitution <= 0.0F ||
      *draft.coefficient_of_restitution > 1.0F) {
    errors.emplace_back("Coefficient of restitution must be in (0, 1]");
  }
  require_non_negative(draft.friction, "Friction");
  require_non_negative(draft.yield_stress, "Yield stress");
  return errors;
}

[[nodiscard]] inline std::optional<SimulationConfig> ValidateSimulationConfig(
    SimulationSettingsDraft const& draft, SharedTerrain terrain) {
  if (!ValidateSimulationSettings(draft).empty() || terrain == nullptr ||
      !terrain->IsValid()) {
    return std::nullopt;
  }

  return SimulationConfig{
      .voxel_max_particles = *draft.voxel_max_particles,
      .rest_density = *draft.rest_density,
      .total_fluid_volume = *draft.total_fluid_volume,
      .viscosity = *draft.viscosity,
      .gas_constant = *draft.gas_constant,
      .coefficient_of_restitution = *draft.coefficient_of_restitution,
      .terrain = std::move(terrain),
      .friction = *draft.friction,
      .yield_stress = *draft.yield_stress,
      .initial_particle_spacing = *draft.initial_particle_spacing,
  };
}

}  // namespace domain

#endif  // RHEO_DOMAIN_SIMULATION_CONFIG_H
