#include <chrono>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rheo/assets/asset_services.h"

namespace {

void Check(bool condition,
           std::source_location location = std::source_location::current()) {
  if (!condition) {
    throw std::runtime_error("test check failed at line " +
                             std::to_string(location.line()));
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
  Check(loaded->simulation.lbm == domain::LbmSettings{});

  Check(loaded->simulation.lattice.height_subdivisions == 13);
  Check(loaded->simulation.lattice.upper_elevation_margin == 0);
  loaded->simulation.lattice = {.height_subdivisions = 27,
                                .upper_elevation_margin = 12.5F};
  loaded->simulation.lbm = {.steps_per_second = 30,
                            .initial_density = 1.1F,
                            .omega = 1.4F,
                            .atmospheric_density = 0.95F,
                            .max_velocity = 0.2F,
                            .fill_offset = 0.004F,
                            .lonely_threshold = 0.2F,
                            .gravity = {0.01F, -0.006F, 0.02F}};
  auto const saved_path = directory / "round-trip.yaml";
  auto saved = repository.Save(saved_path.string(), *loaded);
  Check(saved.has_value());
  auto round_trip = repository.Load(saved_path.string());
  Check(round_trip.has_value());
  Check(round_trip->terrain_path == loaded->terrain_path);
  Check(round_trip->simulation.lbm == loaded->simulation.lbm);

  Check(round_trip->simulation.lattice == loaded->simulation.lattice);
  Check(round_trip->terrain_texture_path == "colors.png");
  {
    std::ifstream saved_file(saved_path);
    std::string yaml((std::istreambuf_iterator<char>(saved_file)), {});
    Check(yaml.find("version: 3") != std::string::npos);
    Check(yaml.find("viscosity") == std::string::npos);
    Check(yaml.find("gravity") != std::string::npos);
  }
  {
    std::ofstream output(legacy_path);
    output << "parameters:\n  height_subdivisions: 0\n";
  }
  Check(!repository.Load(legacy_path.string()));
  loaded->edits = domain::LatticeEdits{std::string(64, 'a'),
                                       {3, 4, 3, 36, 0.31415927F, 3},
                                       {{3, 3, 4}, {15, 1, 3}}};
  Check(repository.Save(saved_path.string(), *loaded).has_value());
  round_trip = repository.Load(saved_path.string());
  Check(round_trip && round_trip->edits &&
        round_trip->edits->runs.size() == 2 &&
        round_trip->edits->runs == loaded->edits->runs);
  Check(round_trip->edits->definition.meters_per_cell ==
        loaded->edits->definition.meters_per_cell);
  auto valid = [&](std::string edits) {
    std::ofstream out(legacy_path);
    out << "version: 2\nparameters: {}\nlattice_edits:\n  dem_sha256: "
        << std::string(64, 'a')
        << "\n  shape: [3, 4, 3]\n  meters_per_cell: 2\n  "
           "terrain_elevation_cells: 3\n  runs: "
        << edits << "\n";
    out.close();
    return repository.Load(legacy_path.string()).has_value();
  };
  Check(!valid("[[3, 1, 256]]"));
  Check(!valid("[[3, 1, 5]]"));
  Check(!valid("[[3, 1, 6]]"));
  Check(!valid("[[3, 2, 4], [4, 1, 3]]"));
  Check(!valid("[[35, 2, 3]]"));
  Check(!valid("[[3, 0, 3]]"));
  Check(!valid("[[3, 1, -1]]"));
  Check(!valid("[[3, -1, 3]]"));
  {
    std::ofstream out(legacy_path);
    out << "version: 3\nparameters:\n  omega: 2\n";
  }
  Check(!repository.Load(legacy_path.string()));
  {
    std::ofstream out(legacy_path);
    out << "version: 2\nparameters: {}\nlattice_edits:\n  dem_sha256: "
        << std::string(64, 'a')
        << "\n  shape: [-1, 4, 3]\n  meters_per_cell: 2\n  "
           "terrain_elevation_cells: 3\n  runs: []\n";
  }
  Check(!repository.Load(legacy_path.string()));
  // Rename failure must preserve the existing destination and clean the
  // temporary.
  auto destination = directory / "existing-directory";
  std::filesystem::create_directory(destination);
  Check(!repository.Save(destination.string(), *loaded));
  Check(std::filesystem::is_directory(destination));
  for (auto const& entry : std::filesystem::directory_iterator(directory))
    Check(entry.path().filename().string().find(".tmp.") == std::string::npos);
  std::filesystem::remove(destination);
  std::filesystem::remove(legacy_path);
  std::filesystem::remove(saved_path);
  std::filesystem::remove(directory);
}
