#define STB_IMAGE_IMPLEMENTATION
#include <gdal.h>
#include <gdal_priv.h>
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "rheo/assets/asset_services.h"

namespace {

std::expected<domain::ImageData, assets::AssetError> LoadWithGdal(
    std::string const& path) {
  GDALAllRegister();
  auto* dataset =
      reinterpret_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
  if (dataset == nullptr) {
    return std::unexpected(assets::AssetError{"Could not decode image"});
  }

  int const width = GDALGetRasterXSize(dataset);
  int const height = GDALGetRasterYSize(dataset);
  int const bands = GDALGetRasterCount(dataset);
  if (width <= 0 || height <= 0 || bands <= 0) {
    GDALClose(dataset);
    return std::unexpected(assets::AssetError{"Image has invalid dimensions"});
  }

  domain::ImageData image{
      .width = static_cast<std::uint32_t>(width),
      .height = static_cast<std::uint32_t>(height),
      .rgba = std::vector<std::uint8_t>(
          static_cast<std::size_t>(width) * height * 4U, 255U)};
  std::vector<std::uint8_t> band_buffer(static_cast<std::size_t>(width) *
                                        height);
  int const color_bands = std::min(bands, 3);
  for (int band_index = 1; band_index <= color_bands; ++band_index) {
    GDALRasterBand* band = dataset->GetRasterBand(band_index);
    if (band == nullptr ||
        band->RasterIO(GF_Read, 0, 0, width, height, band_buffer.data(), width,
                       height, GDT_Byte, 0, 0) != CE_None) {
      GDALClose(dataset);
      return std::unexpected(assets::AssetError{"Could not read image band"});
    }
    for (std::size_t i = 0; i < band_buffer.size(); ++i) {
      if (bands == 1) {
        image.rgba[(4U * i)] = band_buffer[i];
        image.rgba[(4U * i) + 1U] = band_buffer[i];
        image.rgba[(4U * i) + 2U] = band_buffer[i];
      } else {
        image.rgba[(4U * i) + static_cast<std::size_t>(band_index - 1)] =
            band_buffer[i];
      }
    }
  }
  GDALClose(dataset);
  return image;
}

}  // namespace

std::expected<domain::SharedImage, assets::AssetError>
assets::ImageLoader::Load(std::string const& path) const {
  if (path.empty()) {
    return std::unexpected(AssetError{"Image path is empty"});
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels =
      stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  domain::ImageData image;
  if (pixels != nullptr) {
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    std::size_t const size = static_cast<std::size_t>(width) * height * 4U;
    image.rgba.assign(pixels, pixels + size);
    stbi_image_free(pixels);
  } else {
    auto extension = std::filesystem::path(path).extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char character) {
                             return static_cast<char>(std::tolower(character));
                           });
    if (extension != ".tif" && extension != ".tiff") {
      return std::unexpected(AssetError{"Could not decode image"});
    }
    auto result = LoadWithGdal(path);
    if (!result) {
      return std::unexpected(result.error());
    }
    image = std::move(*result);
  }

  if (!image.IsValid()) {
    return std::unexpected(AssetError{"Decoded image is incomplete"});
  }
  return std::make_shared<const domain::ImageData>(std::move(image));
}
