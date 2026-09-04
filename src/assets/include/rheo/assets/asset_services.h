#ifndef RHEO_ASSETS_ASSET_SERVICES_H
#define RHEO_ASSETS_ASSET_SERVICES_H

#include <expected>
#include <string>

#include "rheo/domain/image_data.h"
#include "rheo/domain/project_document.h"
#include "rheo/domain/terrain_data.h"

namespace assets {

struct AssetError {
  std::string message;
};

class ITerrainLoader {
 public:
  virtual ~ITerrainLoader() = default;
  [[nodiscard]] virtual std::expected<domain::SharedTerrain, AssetError> Load(
      std::string const& path, float resolution_meters) const = 0;
};

class IImageLoader {
 public:
  virtual ~IImageLoader() = default;
  [[nodiscard]] virtual std::expected<domain::SharedImage, AssetError> Load(
      std::string const& path) const = 0;
};

class IProjectRepository {
 public:
  virtual ~IProjectRepository() = default;
  [[nodiscard]] virtual std::expected<domain::ProjectDocument, AssetError> Load(
      std::string const& path) const = 0;
  [[nodiscard]] virtual std::expected<void, AssetError> Save(
      std::string const& path,
      domain::ProjectDocument const& document) const = 0;
};

class TerrainLoader final : public ITerrainLoader {
 public:
  [[nodiscard]] std::expected<domain::SharedTerrain, AssetError> Load(
      std::string const& path, float resolution_meters) const override;
};

class ImageLoader final : public IImageLoader {
 public:
  [[nodiscard]] std::expected<domain::SharedImage, AssetError> Load(
      std::string const& path) const override;
};

class ProjectRepository final : public IProjectRepository {
 public:
  [[nodiscard]] std::expected<domain::ProjectDocument, AssetError> Load(
      std::string const& path) const override;
  [[nodiscard]] std::expected<void, AssetError> Save(
      std::string const& path,
      domain::ProjectDocument const& document) const override;
};

}  // namespace assets

#endif  // RHEO_ASSETS_ASSET_SERVICES_H
