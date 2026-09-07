#include <exception>
#include <memory>

#include "geotiff.h"
#include "rheo/assets/asset_services.h"
std::expected<domain::SharedDem, assets::AssetError> assets::DemLoader::Load(
    std::string const& path) const {
  if (path.empty()) return std::unexpected(AssetError{"DEM path is empty"});
  try {
    return std::make_shared<const domain::DemData>(ReadGeoTiff(path));
  } catch (std::exception const& error) {
    return std::unexpected(AssetError{error.what()});
  }
}
