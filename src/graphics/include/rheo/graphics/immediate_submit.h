#ifndef RHEOLBM_IMMEDIATE_SUBMIT_H
#define RHEOLBM_IMMEDIATE_SUBMIT_H

#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/buffer.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/images.h"

namespace graphics {

class ImmediateSubmit {
 public:
  void CopyBuffer(graphics::Device const& device,
                  graphics::CommandPools const& command_pools,
                  graphics::AllocatedBuffer const& src_buffer,
                  graphics::AllocatedBuffer const& dst_buffer,
                  vk::DeviceSize size);
  void CopyBufferToImage(graphics::Device const& device,
                         graphics::CommandPools const& command_pools,
                         graphics::AllocatedBuffer const& buffer,
                         graphics::AllocatedImage const& image, uint32_t width,
                         uint32_t height);
  void TransitionImageLayout(graphics::Device const& device,
                             graphics::CommandPools const& command_pools,
                             graphics::AllocatedImage const& image,
                             vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout);

 private:
  vk::raii::CommandBuffer command_buffer_ = nullptr;

  void BeginSingleTimeCommands(graphics::Device const& device,
                               graphics::CommandPools const& command_pools);
  void EndSingleTimeCommands(graphics::Device const& device);
};

}  // namespace graphics

#endif  // !RHEOLBM_IMMEDIATE_SUBMIT_H
