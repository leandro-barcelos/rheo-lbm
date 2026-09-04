#ifndef RHEOLBM_FLUID_RENDERER_H
#define RHEOLBM_FLUID_RENDERER_H

#include <optional>
#include <vulkan/vulkan_raii.hpp>

#include "camera.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/descriptor.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/simulation/fluid_types.h"

namespace renderer {

class FluidRenderer {
 public:
  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain);
  void Render(vk::raii::CommandBuffer const& command_buffer,
              graphics::SwapChain& swap_chain,
              std::optional<simulation::FluidRenderSnapshot> const& fluid,
              renderer::Camera const& camera);

 private:
  struct CameraUBO {  // NOLINT(altera-struct-pack-align)
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  } __attribute__((aligned(16)));

  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  graphics::AllocatedBuffer camera_ubo_buffer_;
  graphics::DescriptorAllocator camera_descriptor_allocator_;
  vk::raii::DescriptorSetLayout camera_descriptor_set_layout_ = nullptr;
  vk::raii::DescriptorSet camera_descriptor_set_ = nullptr;

  void CreateGraphicsPipeline(graphics::Device const& device,
                              graphics::SwapChain const& swap_chain);
  void CreateCameraDescriptorSetLayout(graphics::Device const& device);
  void CreateBuffers(graphics::Device const& device);
  void CreateCameraDescriptorSet(
      graphics::Device const& device,
      graphics::DescriptorAllocator const& descriptor_allocator);
  void UpdateUniformBuffer(graphics::SwapChain const& swap_chain,
                           renderer::Camera const& camera);
};

}  // namespace renderer

#endif  // !RHEOLBM_FLUID_RENDERER_H
