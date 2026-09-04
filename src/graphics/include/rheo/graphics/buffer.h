#ifndef RHEOLBM_BUFFER_H
#define RHEOLBM_BUFFER_H

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"

namespace graphics {

struct AllocatedBuffer {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Buffer buffer = nullptr;
  void* mapped = nullptr;
} __attribute__((aligned(128)));

class BufferAllocator {
 public:
  [[nodiscard]] static AllocatedBuffer CreateBuffer(
      graphics::Device const& device, vk::DeviceSize size,
      vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

  [[nodiscard]] static AllocatedBuffer CreateMappedUniformBuffer(
      graphics::Device const& device, vk::DeviceSize size);

  static void WriteMapped(AllocatedBuffer const& buffer, void const* data,
                          vk::DeviceSize size) {
    if (buffer.mapped == nullptr) {
      throw std::runtime_error(
          "[ERROR] Graphics: tried writing to an unmapped buffer");
    }
    std::memcpy(static_cast<std::byte*>(buffer.mapped), data,
                static_cast<size_t>(size));
  }

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
