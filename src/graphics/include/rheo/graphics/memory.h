#ifndef RHEOLBM_MEMORY_H
#define RHEOLBM_MEMORY_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/device.h"

namespace graphics {

class MemoryAllocator {
 public:
  [[nodiscard]] static uint32_t FindMemoryType(
      graphics::Device const& device, uint32_t type_filter,
      vk::MemoryPropertyFlags properties);
};

}  // namespace graphics

#endif  // !RHEOLBM_MEMORY_H
