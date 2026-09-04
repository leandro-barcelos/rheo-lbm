#include "rheo/graphics/command_pool.h"

void graphics::CommandPools::Init(graphics::Device const& device) {
  vk::CommandPoolCreateInfo graphics_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = device.GraphicsQueueFamilyIndex(),
  };
  graphics_command_pool_ =
      vk::raii::CommandPool(device.LogicalDevice(), graphics_pool_info);

  vk::CommandPoolCreateInfo compute_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = device.ComputeQueueFamilyIndex(),
  };
  compute_command_pool_ =
      vk::raii::CommandPool(device.LogicalDevice(), compute_pool_info);
}
