#ifndef RHEO_DOMAIN_LATTICE_SETTINGS_H
#define RHEO_DOMAIN_LATTICE_SETTINGS_H
#include <cmath>
#include <cstdint>
#include <expected>
#include <string>
namespace domain {
struct LatticeSettings {
  std::int32_t height_subdivisions = 13;
  float upper_elevation_margin = 0.0F;  // meters
  bool operator==(LatticeSettings const&) const = default;
};
inline std::expected<void, std::string> ValidateLatticeSettings(
    LatticeSettings const& settings) {
  if (settings.height_subdivisions < 1)
    return std::unexpected("Height subdivisions must be greater than zero");
  if (!std::isfinite(settings.upper_elevation_margin) ||
      settings.upper_elevation_margin < 0)
    return std::unexpected(
        "Upper elevation margin must be finite and non-negative");
  return {};
}
}  // namespace domain
#endif
