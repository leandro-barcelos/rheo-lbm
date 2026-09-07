#ifndef RHEO_ASSETS_GEOTIFF_H
#define RHEO_ASSETS_GEOTIFF_H
#include <string>

#include "rheo/domain/dem_data.h"
namespace assets {
// Throws on unsupported metadata or unreadable data; DemLoader translates
// errors.
[[nodiscard]] domain::DemData ReadGeoTiff(std::string const& path);
}  // namespace assets
#endif
