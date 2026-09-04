#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "rheo/assets/asset_services.h"

namespace {

template <typename T>
std::optional<T> ReadOptional(YAML::Node const& node, char const* key) {
  YAML::Node const child = node[key];
  if (!child || child.IsNull()) {
    return std::nullopt;
  }
  return child.as<T>();
}

template <typename T>
void WriteOptional(YAML::Node& node, char const* key,
                   std::optional<T> const& value) {
  if (value) {
    node[key] = *value;
  } else {
    node[key] = YAML::Node{};
  }
}

}  // namespace

std::expected<domain::ProjectDocument, assets::AssetError>
assets::ProjectRepository::Load(std::string const& path) const {
  try {
    YAML::Node const root = YAML::LoadFile(path);
    YAML::Node const parameters = root["parameters"];
    if (!parameters || !parameters.IsMap()) {
      return std::unexpected(AssetError{"Project parameters are missing"});
    }

    domain::ProjectDocument document;
    auto& draft = document.simulation;
    draft.total_fluid_volume =
        ReadOptional<float>(parameters, "total_fluid_volume");
    draft.initial_particle_spacing =
        ReadOptional<float>(parameters, "initial_particle_spacing");
    draft.dem_resolution = ReadOptional<float>(parameters, "dem_resolution");
    draft.voxel_max_particles =
        ReadOptional<std::uint32_t>(parameters, "voxel_max_particles");
    draft.viscosity = ReadOptional<float>(parameters, "viscosity");
    draft.rest_density = ReadOptional<float>(parameters, "rest_density");
    draft.gas_constant = ReadOptional<float>(parameters, "gas_constant");
    draft.coefficient_of_restitution =
        ReadOptional<float>(parameters, "coefficient_of_restitution");
    draft.friction = ReadOptional<float>(parameters, "friction");
    draft.yield_stress = ReadOptional<float>(parameters, "yield_stress");
    document.terrain_path =
        root["elevation_texture_path"]
            ? root["elevation_texture_path"].as<std::string>()
            : std::string{};
    document.terrain_texture_path =
        root["terrain_texture_path"]
            ? root["terrain_texture_path"].as<std::string>()
            : std::string{};
    return document;
  } catch (std::exception const& error) {
    return std::unexpected(AssetError{error.what()});
  }
}

std::expected<void, assets::AssetError> assets::ProjectRepository::Save(
    std::string const& path, domain::ProjectDocument const& document) const {
  try {
    std::filesystem::path const output_path(path);
    if (output_path.has_parent_path()) {
      std::filesystem::create_directories(output_path.parent_path());
    }

    YAML::Node root;
    root["version"] = 1;
    root["elevation_texture_path"] = document.terrain_path;
    root["terrain_texture_path"] = document.terrain_texture_path;
    YAML::Node parameters;
    auto const& draft = document.simulation;
    WriteOptional(parameters, "total_fluid_volume", draft.total_fluid_volume);
    WriteOptional(parameters, "initial_particle_spacing",
                  draft.initial_particle_spacing);
    WriteOptional(parameters, "voxel_max_particles", draft.voxel_max_particles);
    WriteOptional(parameters, "viscosity", draft.viscosity);
    WriteOptional(parameters, "rest_density", draft.rest_density);
    WriteOptional(parameters, "gas_constant", draft.gas_constant);
    WriteOptional(parameters, "coefficient_of_restitution",
                  draft.coefficient_of_restitution);
    WriteOptional(parameters, "friction", draft.friction);
    WriteOptional(parameters, "yield_stress", draft.yield_stress);
    WriteOptional(parameters, "dem_resolution", draft.dem_resolution);
    root["parameters"] = parameters;

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
      return std::unexpected(AssetError{"Could not open project for writing"});
    }
    output << root;
    if (!output.good()) {
      return std::unexpected(AssetError{"Could not write project"});
    }
    return {};
  } catch (std::exception const& error) {
    return std::unexpected(AssetError{error.what()});
  }
}
