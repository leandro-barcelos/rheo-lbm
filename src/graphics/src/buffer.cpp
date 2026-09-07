#include "rheo/graphics/buffer.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rheo/graphics/immediate_submit.h"
#include "rheo/graphics/memory.h"

graphics::AllocatedBuffer graphics::BufferAllocator::CreateBuffer(
    graphics::Device const& device, vk::DeviceSize size,
    vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  std::vector<uint32_t> queue_family_indices{device.ComputeQueueFamilyIndex(),
                                             device.GraphicsQueueFamilyIndex()};

  bool const shared = queue_family_indices[0] != queue_family_indices[1];
  vk::BufferCreateInfo buffer_info{
      .size = size,
      .usage = usage,
      .sharingMode =
          shared ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
      .queueFamilyIndexCount =
          shared ? static_cast<uint32_t>(queue_family_indices.size()) : 0U,
      .pQueueFamilyIndices = queue_family_indices.data(),
  };

  vk::raii::Buffer buffer{device.LogicalDevice(), buffer_info};
  vk::MemoryRequirements mem_requirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo alloc_info{
      .allocationSize = mem_requirements.size,
      .memoryTypeIndex = MemoryAllocator::FindMemoryType(
          device, mem_requirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory memory{device.LogicalDevice(), alloc_info};
  buffer.bindMemory(memory, 0);

  return graphics::AllocatedBuffer{
      .memory = std::move(memory),
      .buffer = std::move(buffer),
  };
}

graphics::AllocatedBuffer graphics::BufferAllocator::CreateMappedUniformBuffer(
    graphics::Device const& device, vk::DeviceSize size) {
  auto uniform_buffer =
      CreateBuffer(device, size, vk::BufferUsageFlagBits::eUniformBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);
  uniform_buffer.mapped = uniform_buffer.memory.mapMemory(0, size);
  return uniform_buffer;
}

graphics::AllocatedBuffer graphics::BufferAllocator::CreateUniformBufferBytes(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    void const* data, vk::DeviceSize buffer_size) {
  graphics::AllocatedBuffer staging_buffer =
      CreateBuffer(device, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data_staging = staging_buffer.memory.mapMemory(0, buffer_size);
  std::memcpy(data_staging, data, static_cast<size_t>(buffer_size));
  staging_buffer.memory.unmapMemory();

  graphics::AllocatedBuffer storage_buffer =
      CreateBuffer(device, buffer_size,
                   vk::BufferUsageFlagBits::eUniformBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);
  ImmediateSubmit single_time_command;
  single_time_command.CopyBuffer(device, command_pools, staging_buffer,
                                 storage_buffer, buffer_size);

  return storage_buffer;
}

std::array<graphics::AllocatedBuffer, 2>
graphics::BufferAllocator::CreateStorageBuffers(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    void const* data, vk::DeviceSize size, bool double_buffering,
    vk::BufferUsageFlags extra_usage_flags) {
  if (data == nullptr || size == 0) {
    throw std::runtime_error(
        "[ERROR] Vulkan: Tried creating a buffer with size 0!");
  }

  graphics::AllocatedBuffer staging_buffer =
      CreateBuffer(device, size, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data_staging = staging_buffer.memory.mapMemory(0, size);
  std::memcpy(data_staging, data, static_cast<size_t>(size));
  staging_buffer.memory.unmapMemory();

  std::array<graphics::AllocatedBuffer, 2> storage_buffers;
  for (size_t i = 0; i < 2; i++) {
    if (i == 1 && !double_buffering) {
      storage_buffers.at(i) = {.memory = nullptr, .buffer = nullptr};
      continue;
    }

    storage_buffers.at(i) = CreateBuffer(
        device, size,
        vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst | extra_usage_flags,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    ImmediateSubmit single_time_command;
    single_time_command.CopyBuffer(device, command_pools, staging_buffer,
                                   storage_buffers.at(i), size);
  }

  return storage_buffers;
}
