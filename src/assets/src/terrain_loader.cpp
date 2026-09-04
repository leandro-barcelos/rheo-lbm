#include <array>
#include <cstdint>
#include <memory>
#include <utility>

#include "geotiff.h"
#include "rheo/assets/asset_services.h"

std::expected<domain::SharedTerrain, assets::AssetError>
assets::TerrainLoader::Load(std::string const& path,
                            float resolution_meters) const {
  if (path.empty()) {
    return std::unexpected(AssetError{"Terrain path is empty"});
  }

  GeoTiff source(path);
  std::array<int, 3> const dimensions = source.Dimensions();
  if (dimensions[0] <= 0 || dimensions[1] <= 0 || dimensions[2] <= 0) {
    return std::unexpected(AssetError{"Could not read terrain dimensions"});
  }

  auto samples = source.Elevations(1, resolution_meters);
  auto terrain = std::make_shared<domain::TerrainData>(domain::TerrainData{
      .samples = std::move(samples),
      .width = static_cast<std::uint32_t>(dimensions[0]),
      .height = static_cast<std::uint32_t>(dimensions[1]),
  });
  if (!terrain->IsValid()) {
    return std::unexpected(AssetError{"Terrain data is incomplete"});
  }
  return terrain;
}
