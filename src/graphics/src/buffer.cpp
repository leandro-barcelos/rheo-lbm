#include "rheo/graphics/buffer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rheo/graphics/immediate_submit.h"
#include "rheo/graphics/memory.h"

graphics::AllocatedBuffer::~AllocatedBuffer() {
  if (allocator_ != nullptr) {
    vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(buffer_.release()),
                     allocation_);
  }
}

graphics::AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& other) noexcept {
  Swap(other);
}

graphics::AllocatedBuffer& graphics::AllocatedBuffer::operator=(
    AllocatedBuffer&& other) noexcept {
  if (this != &other) {
    AllocatedBuffer previous(std::move(other));
    Swap(previous);
  }
  return *this;
}

void graphics::AllocatedBuffer::Swap(AllocatedBuffer& other) noexcept {
  std::swap(allocator_, other.allocator_);
  std::swap(allocation_, other.allocation_);
  buffer_.swap(other.buffer_);
  std::swap(mapped_, other.mapped_);
  std::swap(size_, other.size_);
}

void graphics::AllocatedBuffer::CheckMappedRange(vk::DeviceSize size) const {
  if ((mapped_ == nullptr) || size > size_) {
    throw std::runtime_error("[ERROR] Graphics: invalid mapped buffer range");
  }
}

void graphics::AllocatedBuffer::Flush(vk::DeviceSize size) const {
  CheckMappedRange(size);
  auto result = vmaFlushAllocation(allocator_, allocation_, 0, size);
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::format(
        "[ERROR] Graphics: failed to flush buffer ({})", int(result)));
  }
}

void const* graphics::AllocatedBuffer::ReadMapped(vk::DeviceSize size) const {
  CheckMappedRange(size);
  auto result = vmaInvalidateAllocation(allocator_, allocation_, 0, size);
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::format(
        "[ERROR] Graphics: failed to invalidate buffer ({})", int(result)));
  }
  return mapped_;
}

void* graphics::AllocatedBuffer::Mapped(vk::DeviceSize size) const {
  CheckMappedRange(size);
  return mapped_;
}

void graphics::AllocatedBuffer::WriteMapped(void const* data,
                                            vk::DeviceSize size) const {
  CheckMappedRange(size);
  if ((data == nullptr) && (size != 0U)) {
    throw std::invalid_argument("[ERROR] Graphics: null buffer upload data");
  }
  if (size != 0U) {
    std::memcpy(mapped_, data, static_cast<std::size_t>(size));
  }
  Flush(size);
}

graphics::AllocatedBuffer graphics::BufferAllocator::CreateBuffer(
    graphics::Device const& device, vk::DeviceSize size,
    vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  if (size == 0) {
    throw std::invalid_argument(
        "[ERROR] Graphics: buffer size must be positive");
  }
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

  VmaAllocationCreateInfo alloc_info{
      .usage = VMA_MEMORY_USAGE_AUTO,
      .requiredFlags = static_cast<VkMemoryPropertyFlags>(properties),
  };
  if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                       VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  AllocatedBuffer result;
  result.allocator_ = device.Allocator();
  result.size_ = size;
  VkBuffer raw_buffer = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info{};
  auto status = vmaCreateBuffer(
      result.allocator_,
      reinterpret_cast<VkBufferCreateInfo const*>(&buffer_info), &alloc_info,
      &raw_buffer, &result.allocation_, &allocation_info);
  if (status != VK_SUCCESS) {
    throw std::runtime_error(std::format(
        "[ERROR] Graphics: failed to allocate buffer ({})", int(status)));
  }
  result.buffer_ = vk::raii::Buffer(device.LogicalDevice(), raw_buffer);
  result.mapped_ = allocation_info.pMappedData;
  return result;
}

graphics::AllocatedBuffer graphics::BufferAllocator::CreateMappedUniformBuffer(
    graphics::Device const& device, vk::DeviceSize size) {
  return CreateBuffer(device, size, vk::BufferUsageFlagBits::eUniformBuffer,
                      vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent);
}

graphics::AllocatedBuffer graphics::BufferAllocator::CreateUniformBufferBytes(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    void const* data, vk::DeviceSize buffer_size) {
  graphics::AllocatedBuffer staging_buffer =
      CreateBuffer(device, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  staging_buffer.WriteMapped(data, buffer_size);

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

  staging_buffer.WriteMapped(data, size);

  std::array<graphics::AllocatedBuffer, 2> storage_buffers;
  for (size_t i = 0; i < 2; i++) {
    if (i == 1 && !double_buffering) {
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
