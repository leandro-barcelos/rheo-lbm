#include "rheo/domain/lattice_definition.h"

#include <limits>
#include <stdexcept>
void Check(bool value) {
  if (!value) throw std::runtime_error("lattice definition check failed");
}
int main() {
  domain::DemData dem{.samples = std::vector<domain::Elevation>(6),
                      .width = 3,
                      .height = 2,
                      .pixel_size_meters = {10, 20},
                      .min_elevation = -10,
                      .max_elevation = 16};
  auto lattice = domain::DefineLattice(dem, {});
  Check(lattice.has_value());
  Check(lattice->meters_per_cell == 2 && lattice->width == 15 &&
        lattice->height == 13 && lattice->depth == 20 &&
        lattice->cell_count == 3900);
  lattice = domain::DefineLattice(
      dem, {.height_subdivisions = 13, .upper_elevation_margin = 0.5F});
  Check(lattice->height == 14 && lattice->terrain_elevation_cells == 13);
  dem.max_elevation = dem.min_elevation;
  lattice = domain::DefineLattice(dem, {});
  Check(lattice->meters_per_cell == 20 && lattice->width == 2 &&
        lattice->depth == 2 && lattice->height == 13);
  Check(!domain::DefineLattice(dem, {.height_subdivisions = 0}));
  Check(!domain::DefineLattice(dem, {.upper_elevation_margin = -1}));
  Check(!domain::DefineLattice(
      dem, {.upper_elevation_margin = std::numeric_limits<float>::infinity()}));
  Check(!domain::DefineLattice(
      dem, {.upper_elevation_margin = std::numeric_limits<float>::max()}));
  dem.pixel_size_meters.x = 0;
  Check(!domain::DefineLattice(dem, {}));
}
