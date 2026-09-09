#ifndef RHEOLBM_BUFFER_H
#define RHEOLBM_BUFFER_H

#include <array>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/memory.h"

namespace graphics {

class AllocatedBuffer {
 public:
  AllocatedBuffer() = default;
  ~AllocatedBuffer();
  AllocatedBuffer(AllocatedBuffer const&) = delete;
  AllocatedBuffer& operator=(AllocatedBuffer const&) = delete;
  AllocatedBuffer(AllocatedBuffer&& other) noexcept;
  AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept;

  [[nodiscard]] vk::raii::Buffer const& Buffer() const { return buffer_; }
  [[nodiscard]] void* Mapped(vk::DeviceSize size) const;
  void WriteMapped(void const* data, vk::DeviceSize size) const;
  void Flush(vk::DeviceSize size) const;
  [[nodiscard]] void const* ReadMapped(vk::DeviceSize size) const;

 private:
  friend class BufferAllocator;
  void Swap(AllocatedBuffer& other) noexcept;
  void CheckMappedRange(vk::DeviceSize size) const;

  VmaAllocation allocation_ = nullptr;
  vk::raii::Buffer buffer_ = nullptr;
  void* mapped_ = nullptr;
  VmaAllocator allocator_ = nullptr;
  vk::DeviceSize size_ = 0;
};

class BufferAllocator {
 public:
  [[nodiscard]] static AllocatedBuffer CreateBuffer(
      graphics::Device const& device, vk::DeviceSize size,
      vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

  [[nodiscard]] static AllocatedBuffer CreateMappedUniformBuffer(
      graphics::Device const& device, vk::DeviceSize size);

  template <typename T>
  [[nodiscard]] static graphics::AllocatedBuffer CreateUniformBuffer(
      graphics::Device const& device,
      graphics::CommandPools const& command_pools, T const& data) {
    return CreateUniformBufferBytes(device, command_pools, &data, sizeof(T));
  }

  template <typename T>
  [[nodiscard]] static std::array<graphics::AllocatedBuffer, 2> CreateSSBO(
      graphics::Device const& device,
      graphics::CommandPools const& command_pools,
      std::vector<T> const& objects, bool double_buffering = true,
      vk::BufferUsageFlags extra_usage_flags = {}) {
    return CreateStorageBuffers(device, command_pools, objects.data(),
                                sizeof(T) * objects.size(), double_buffering,
                                extra_usage_flags);
  }

 private:
  [[nodiscard]] static AllocatedBuffer CreateUniformBufferBytes(
      Device const& device, CommandPools const& command_pools, void const* data,
      vk::DeviceSize size);
  [[nodiscard]] static std::array<AllocatedBuffer, 2> CreateStorageBuffers(
      Device const& device, CommandPools const& command_pools, void const* data,
      vk::DeviceSize size, bool double_buffering,
      vk::BufferUsageFlags extra_usage_flags);
};

}  // namespace graphics

#endif  // !RHEOLBM_BUFFER_H
