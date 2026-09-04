#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "rheo/assets/asset_services.h"

namespace {

void Check(bool condition) {
  if (!condition) {
    throw std::runtime_error("test check failed");
  }
}

}  // namespace

int main() {
  auto const suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  auto const directory = std::filesystem::temp_directory_path() /
                         ("rheo-lbm-assets-test-" + suffix);
  std::filesystem::create_directories(directory);
  auto const legacy_path = directory / "legacy-v1.yaml";

  {
    std::ofstream output(legacy_path);
    output << "version: 1\n"
              "elevation_texture_path: terrain.tif\n"
              "terrain_texture_path: colors.png\n"
              "parameters:\n"
              "  total_fluid_volume: 1000\n"
              "  initial_particle_spacing: 0.1\n"
              "  dem_resolution: 5\n"
              "  voxel_max_particles: 16\n"
              "  viscosity: 10\n"
              "  rest_density: 1000\n"
              "  gas_constant: 20\n"
              "  coefficient_of_restitution: 0.5\n"
              "  friction: 0.01\n"
              "  yield_stress: 2\n";
  }

  assets::ProjectRepository repository;
  auto loaded = repository.Load(legacy_path.string());
  Check(loaded.has_value());
  Check(loaded->terrain_path == "terrain.tif");
  Check(loaded->terrain_texture_path == "colors.png");
  Check(loaded->simulation.voxel_max_particles == 16);
  Check(loaded->simulation.dem_resolution == 5.0F);

  auto const saved_path = directory / "round-trip.yaml";
  auto saved = repository.Save(saved_path.string(), *loaded);
  Check(saved.has_value());
  auto round_trip = repository.Load(saved_path.string());
  Check(round_trip.has_value());
  Check(round_trip->terrain_path == loaded->terrain_path);
  Check(round_trip->simulation.yield_stress == loaded->simulation.yield_stress);

  std::filesystem::remove(legacy_path);
  std::filesystem::remove(saved_path);
  std::filesystem::remove(directory);
}
