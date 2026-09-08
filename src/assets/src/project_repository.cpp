#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
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

}  // namespace

std::expected<domain::ProjectDocument, assets::AssetError>
assets::ProjectRepository::Load(std::string const& path) const {
  try {
    YAML::Node const root = YAML::LoadFile(path);
    int version = root["version"] ? root["version"].as<int>() : 1;
    if (version < 1 || version > 3)
      throw std::runtime_error("Unsupported project version");
    YAML::Node const parameters = root["parameters"];
    if (!parameters || !parameters.IsMap()) {
      return std::unexpected(AssetError{"Project parameters are missing"});
    }

    domain::ProjectDocument document;
    auto& draft = document.simulation;
    if (version == 3) {
      auto& lbm = draft.lbm;
      lbm.steps_per_second =
          ReadOptional<float>(parameters, "steps_per_second").value_or(15.0F);
      lbm.initial_density =
          ReadOptional<float>(parameters, "initial_density").value_or(1.0F);
      lbm.omega = ReadOptional<float>(parameters, "omega").value_or(1.25F);
      lbm.atmospheric_density =
          ReadOptional<float>(parameters, "atmospheric_density").value_or(1.0F);
      lbm.max_velocity =
          ReadOptional<float>(parameters, "max_velocity").value_or(0.25F);
      lbm.fill_offset =
          ReadOptional<float>(parameters, "fill_offset").value_or(0.003F);
      lbm.lonely_threshold =
          ReadOptional<float>(parameters, "lonely_threshold").value_or(0.1F);
      auto gravity = parameters["gravity"];
      if (gravity) {
        if (!gravity.IsSequence() || gravity.size() != 3)
          throw std::runtime_error("Invalid gravity vector");
        lbm.gravity = {gravity[0].as<float>(), gravity[1].as<float>(),
                       gravity[2].as<float>()};
      }
      auto errors = domain::ValidateLbmSettings(lbm);
      if (!errors.empty())
        return std::unexpected(
            AssetError{"Invalid LBM parameters: " + errors.front()});
    }
    draft.lattice.height_subdivisions =
        ReadOptional<std::int32_t>(parameters, "height_subdivisions")
            .value_or(13);
    draft.lattice.upper_elevation_margin =
        ReadOptional<float>(parameters, "upper_elevation_margin")
            .value_or(0.0F);
    if (auto valid = domain::ValidateLatticeSettings(draft.lattice); !valid)
      return std::unexpected(AssetError{valid.error()});
    document.terrain_path =
        root["elevation_texture_path"]
            ? root["elevation_texture_path"].as<std::string>()
            : std::string{};
    document.terrain_texture_path =
        root["terrain_texture_path"]
            ? root["terrain_texture_path"].as<std::string>()
            : std::string{};
    if (version >= 2 && root["lattice_edits"]) {
      auto node = root["lattice_edits"];
      domain::LatticeEdits edits;
      edits.dem_fingerprint = node["dem_sha256"].as<std::string>();
      if (edits.dem_fingerprint.size() != 64 ||
          edits.dem_fingerprint.find_first_not_of("0123456789abcdef") !=
              std::string::npos)
        throw std::runtime_error("Invalid DEM fingerprint");
      auto shape = node["shape"];
      if (!shape.IsSequence() || shape.size() != 3)
        throw std::runtime_error("Invalid lattice shape");
      auto& d = edits.definition;
      d.width = shape[0].as<std::uint32_t>();
      d.height = shape[1].as<std::uint32_t>();
      d.depth = shape[2].as<std::uint32_t>();
      std::uint64_t count = std::uint64_t(d.width) * d.height;
      if (!d.width || !d.height || !d.depth || count > UINT32_MAX ||
          count * d.depth > UINT32_MAX)
        throw std::runtime_error("Invalid lattice size");
      d.cell_count = count * d.depth;
      d.meters_per_cell = node["meters_per_cell"].as<float>();
      d.terrain_elevation_cells = node["terrain_elevation_cells"].as<float>();
      if (!std::isfinite(d.meters_per_cell) || d.meters_per_cell <= 0 ||
          !std::isfinite(d.terrain_elevation_cells) ||
          d.terrain_elevation_cells < 0)
        throw std::runtime_error("Invalid lattice scale");
      auto runs = node["runs"];
      if (!runs.IsSequence()) throw std::runtime_error("Invalid edit runs");
      std::uint64_t end = 0;
      for (auto run : runs) {
        if (!run.IsSequence() || run.size() != 3)
          throw std::runtime_error("Invalid edit run");
        auto start = run[0].as<std::uint32_t>();
        auto size = run[1].as<std::uint32_t>();
        auto type = run[2].as<int>();
        if (type < 0 || type > 4 ||
            !domain::IsPersistentCellType(static_cast<std::uint8_t>(type)) ||
            !size || start < end || std::uint64_t(start) + size > d.cell_count)
          throw std::runtime_error("Invalid edit range or type");
        end = std::uint64_t(start) + size;
        edits.runs.push_back({start, size, static_cast<std::uint8_t>(type)});
      }
      document.edits = std::move(edits);
    }
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
    root["version"] = 3;
    root["elevation_texture_path"] = document.terrain_path;
    root["terrain_texture_path"] = document.terrain_texture_path;
    YAML::Node parameters;
    auto const& draft = document.simulation;
    auto errors = domain::ValidateLbmSettings(draft.lbm);
    if (!errors.empty())
      throw std::runtime_error("Invalid LBM parameters: " + errors.front());
    parameters["steps_per_second"] = draft.lbm.steps_per_second;
    parameters["initial_density"] = draft.lbm.initial_density;
    parameters["omega"] = draft.lbm.omega;
    parameters["atmospheric_density"] = draft.lbm.atmospheric_density;
    parameters["max_velocity"] = draft.lbm.max_velocity;
    parameters["fill_offset"] = draft.lbm.fill_offset;
    parameters["lonely_threshold"] = draft.lbm.lonely_threshold;
    for (float component :
         {draft.lbm.gravity.x, draft.lbm.gravity.y, draft.lbm.gravity.z})
      parameters["gravity"].push_back(component);
    parameters["height_subdivisions"] = draft.lattice.height_subdivisions;
    parameters["upper_elevation_margin"] = draft.lattice.upper_elevation_margin;
    root["parameters"] = parameters;

    if (document.edits) {
      auto const& e = *document.edits;
      auto const& d = e.definition;
      YAML::Node node;
      node["dem_sha256"] = e.dem_fingerprint;
      for (auto size : {d.width, d.height, d.depth})
        node["shape"].push_back(size);
      node["meters_per_cell"] = d.meters_per_cell;
      node["terrain_elevation_cells"] = d.terrain_elevation_cells;
      node["runs"] = YAML::Node(YAML::NodeType::Sequence);
      for (auto run : e.runs) {
        YAML::Node r;
        r.push_back(run.start);
        r.push_back(run.count);
        r.push_back(int(run.type));
        node["runs"].push_back(r);
      }
      root["lattice_edits"] = node;
    }
    std::string temporary = path + ".tmp.XXXXXX";
    int fd = mkstemp(temporary.data());
    if (fd < 0) throw std::runtime_error("Could not create temporary project");
    close(fd);
    try {
      std::ofstream output(temporary, std::ios::trunc);
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output << root;
      output.flush();
      output.close();
      std::filesystem::rename(temporary, path);
    } catch (...) {
      std::filesystem::remove(temporary);
      throw;
    }
    return {};
  } catch (std::exception const& error) {
    return std::unexpected(AssetError{error.what()});
  }
}
