#ifndef RHEO_ASSETS_ASSET_SERVICES_H
#define RHEO_ASSETS_ASSET_SERVICES_H

#include <expected>
#include <string>

#include "rheo/domain/dem_data.h"
#include "rheo/domain/image_data.h"
#include "rheo/domain/project_document.h"

namespace assets {

struct AssetError {
  std::string message;
} __attribute__((aligned(32)));

class IDemLoader {
 public:
  IDemLoader() = default;
  IDemLoader(const IDemLoader&) = default;
  IDemLoader(IDemLoader&&) = delete;
  IDemLoader& operator=(const IDemLoader&) = default;
  IDemLoader& operator=(IDemLoader&&) = delete;
  virtual ~IDemLoader() = default;
  [[nodiscard]] virtual std::expected<domain::SharedDem, AssetError> Load(
      std::string const& path) const = 0;
};

class IImageLoader {
 public:
  IImageLoader() = default;
  IImageLoader(const IImageLoader&) = default;
  IImageLoader(IImageLoader&&) = delete;
  IImageLoader& operator=(const IImageLoader&) = default;
  IImageLoader& operator=(IImageLoader&&) = delete;
  virtual ~IImageLoader() = default;
  [[nodiscard]] virtual std::expected<domain::SharedImage, AssetError> Load(
      std::string const& path) const = 0;
};

class IProjectRepository {
 public:
  IProjectRepository() = default;
  IProjectRepository(const IProjectRepository&) = default;
  IProjectRepository(IProjectRepository&&) = delete;
  IProjectRepository& operator=(const IProjectRepository&) = default;
  IProjectRepository& operator=(IProjectRepository&&) = delete;
  virtual ~IProjectRepository() = default;
  [[nodiscard]] virtual std::expected<domain::ProjectDocument, AssetError> Load(
      std::string const& path) const = 0;
  [[nodiscard]] virtual std::expected<void, AssetError> Save(
      std::string const& path,
      domain::ProjectDocument const& document) const = 0;
};

class DemLoader final : public IDemLoader {
 public:
  [[nodiscard]] std::expected<domain::SharedDem, AssetError> Load(
      std::string const& path) const override;
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
