#ifndef RHEOAPH_COMMAND_POOL_H
#define RHEOAPH_COMMAND_POOL_H

#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/device.h"

namespace graphics {

class CommandPools {
 public:
  void Init(graphics::Device const& device);

  [[nodiscard]] vk::raii::CommandPool const& Graphics() const {
    return graphics_command_pool_;
  }
  [[nodiscard]] vk::raii::CommandPool const& Compute() const {
    return compute_command_pool_;
  }

 private:
  vk::raii::CommandPool graphics_command_pool_ = nullptr;
  vk::raii::CommandPool compute_command_pool_ = nullptr;
};

}  // namespace graphics

#endif  // !RHEOAPH_COMMAND_POOL_H
