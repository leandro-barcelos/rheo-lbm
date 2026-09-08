#ifndef RHEO_DOMAIN_SIMULATION_CONFIG_H
#define RHEO_DOMAIN_SIMULATION_CONFIG_H

#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "rheo/domain/lattice_settings.h"

namespace domain {

struct LbmSettings {
  float steps_per_second = 15.0F;
  float initial_density = 1.0F;
  float omega = 1.25F;
  float atmospheric_density = 1.0F;
  float max_velocity = 0.25F;
  float fill_offset = 0.003F;
  float lonely_threshold = 0.1F;
  glm::vec3 gravity{0.0F, -0.005F, 0.0F};
  bool operator==(LbmSettings const&) const = default;
};

struct SimulationSettingsDraft {
  LatticeSettings lattice;
  LbmSettings lbm;
  bool operator==(SimulationSettingsDraft const&) const = default;
};

[[nodiscard]] inline std::vector<std::string> ValidateLbmSettings(
    LbmSettings const& settings) {
  std::vector<std::string> errors;
  auto positive = [&errors](float value, char const* name) {
    if (!std::isfinite(value) || value <= 0.0F)
      errors.emplace_back(std::string(name) + " must be finite and positive");
  };
  positive(settings.steps_per_second, "Steps per second");
  positive(settings.initial_density, "Initial density");
  positive(settings.atmospheric_density, "Atmospheric density");
  positive(settings.max_velocity, "Maximum velocity");
  if (!std::isfinite(settings.omega) || settings.omega <= 0.0F ||
      settings.omega >= 2.0F)
    errors.emplace_back("Omega must be finite and in (0, 2)");
  if (!std::isfinite(settings.fill_offset) || settings.fill_offset < 0.0F)
    errors.emplace_back("Fill offset must be finite and non-negative");
  if (!std::isfinite(settings.lonely_threshold) ||
      settings.lonely_threshold < 0.0F || settings.lonely_threshold > 1.0F)
    errors.emplace_back("Lonely threshold must be finite and in [0, 1]");
  if (!std::isfinite(settings.gravity.x) ||
      !std::isfinite(settings.gravity.y) || !std::isfinite(settings.gravity.z))
    errors.emplace_back("Gravity components must be finite");
  return errors;
}

[[nodiscard]] inline std::vector<std::string> ValidateSimulationSettings(
    SimulationSettingsDraft const& draft) {
  auto errors = ValidateLbmSettings(draft.lbm);
  if (auto lattice = ValidateLatticeSettings(draft.lattice); !lattice)
    errors.push_back(lattice.error());
  return errors;
}

}  // namespace domain

#endif  // RHEO_DOMAIN_SIMULATION_CONFIG_H
