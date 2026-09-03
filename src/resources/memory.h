#ifndef RHEOLBM_MEMORY_H
#define RHEOLBM_MEMORY_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"

namespace resources {

class MemoryAllocator {
 public:
  [[nodiscard]] static uint32_t FindMemoryType(
      core::VulkanDevice const& vulkan_device, uint32_t type_filter,
      vk::MemoryPropertyFlags properties);
};

}  // namespace resources

#endif  // !RHEOLBM_MEMORY_H
