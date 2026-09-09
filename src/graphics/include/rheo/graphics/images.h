#ifndef RHEO_GRAPHICS_IMAGES_H
#define RHEO_GRAPHICS_IMAGES_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/domain/image_data.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"

namespace graphics {

class AllocatedImage {
 public:
  AllocatedImage() = default;
  ~AllocatedImage();
  AllocatedImage(AllocatedImage const&) = delete;
  AllocatedImage& operator=(AllocatedImage const&) = delete;
  AllocatedImage(AllocatedImage&& other) noexcept;
  AllocatedImage& operator=(AllocatedImage&& other) noexcept;

  [[nodiscard]] vk::raii::ImageView const& ImageView() const {
    return image_view_;
  }
  [[nodiscard]] vk::raii::Sampler const& Sampler() const { return sampler_; }
  [[nodiscard]] vk::raii::Image const& Image() const { return image_; }

 private:
  friend class ImageAllocator;
  void Swap(AllocatedImage& other) noexcept;

  VmaAllocation allocation_ = nullptr;
  vk::raii::ImageView image_view_ = nullptr;
  vk::raii::Sampler sampler_ = nullptr;
  vk::raii::Image image_ = nullptr;
  VmaAllocator allocator_ = nullptr;
};

class ImageAllocator {
 public:
  [[nodiscard]] static AllocatedImage CreateImage(
      Device const& device, vk::ImageCreateInfo const& image_info);
  [[nodiscard]] static AllocatedImage CreateImage(
      Device const& device, CommandPools const& command_pools,
      domain::ImageData const& image);
  [[nodiscard]] static AllocatedImage CreateSolidColorImage(
      Device const& device, CommandPools const& command_pools, std::uint8_t red,
      std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
  [[nodiscard]] static AllocatedImage CreateDepthImage(Device const& device,
                                                       vk::Extent2D extent,
                                                       vk::Format format);

 private:
  [[nodiscard]] static AllocatedImage UploadImage(
      Device const& device, CommandPools const& command_pools,
      void const* pixels, std::uint32_t width, std::uint32_t height,
      vk::DeviceSize size);
};

}  // namespace graphics

#endif  // RHEO_GRAPHICS_IMAGES_H
