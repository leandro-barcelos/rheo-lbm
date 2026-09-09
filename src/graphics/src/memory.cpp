#include "rheo/graphics/memory.h"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "rheo/graphics/context.h"
#include "rheo/graphics/device.h"

graphics::MemoryAllocator::~MemoryAllocator() {
  if (allocator_ != nullptr) {
    vmaDestroyAllocator(allocator_);
  }
}

void graphics::MemoryAllocator::Init(graphics::GraphicsContext const& context,
                                     graphics::Device const& device) {
  if (allocator_ != nullptr) {
    throw std::logic_error("[ERROR] Graphics: allocator already initialized");
  }
  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr =
      context.Context().getDispatcher()->vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr =
      context.Instance().getDispatcher()->vkGetDeviceProcAddr;
  VmaAllocatorCreateInfo create_info{
      .physicalDevice = *device.PhysicalDevice(),
      .device = *device.LogicalDevice(),
      .pVulkanFunctions = &functions,
      .instance = *context.Instance(),
      .vulkanApiVersion =
          std::min(VK_API_VERSION_1_4,
                   device.PhysicalDevice().getProperties().apiVersion),
  };
  auto result = vmaCreateAllocator(&create_info, &allocator_);
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::format(
        "[ERROR] Graphics: failed to create VMA allocator ({})", int(result)));
  }
}
