#ifndef RHEO_GRAPHICS_IMAGES_H
#define RHEO_GRAPHICS_IMAGES_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/domain/image_data.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"

namespace graphics {

struct AllocatedImage {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::ImageView image_view = nullptr;
  vk::raii::Sampler sampler = nullptr;
  vk::raii::Image image = nullptr;
};

class ImageAllocator {
 public:
  [[nodiscard]] static AllocatedImage CreateImage(
      Device const& device, CommandPools const& command_pools,
      domain::ImageData const& image);
  [[nodiscard]] static AllocatedImage CreateSolidColorImage(
      Device const& device, CommandPools const& command_pools, std::uint8_t red,
      std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
};

}  // namespace graphics

#endif  // RHEO_GRAPHICS_IMAGES_H
