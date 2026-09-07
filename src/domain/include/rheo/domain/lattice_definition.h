#ifndef RHEO_DOMAIN_LATTICE_DEFINITION_H
#define RHEO_DOMAIN_LATTICE_DEFINITION_H
#include <cstdint>
#include <expected>
#include <string>

#include "rheo/domain/dem_data.h"
#include "rheo/domain/lattice_settings.h"
namespace domain {
struct LatticeDefinition {
  std::uint32_t width, height, depth, cell_count;
  float meters_per_cell, terrain_elevation_cells;
};
[[nodiscard]] std::expected<LatticeDefinition, std::string> DefineLattice(
    DemData const& dem, LatticeSettings const& settings);
}  // namespace domain
#endif
