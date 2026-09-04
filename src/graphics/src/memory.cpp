#include "rheo/graphics/memory.h"

#include <stdexcept>

uint32_t graphics::MemoryAllocator::FindMemoryType(
    graphics::Device const& device, uint32_t type_filter,
    vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties mem_properties =
      device.PhysicalDevice().getMemoryProperties();

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if (((type_filter & (1 << i)) != 0U) &&
        (mem_properties.memoryTypes.at(i).propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  throw std::runtime_error(
      "[ERROR] Vulkan: failed to find suitable memory type!");
}
