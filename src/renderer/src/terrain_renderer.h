#ifndef RHEOLBM_TERRAIN_RENDERER_H
#define RHEOLBM_TERRAIN_RENDERER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "camera.h"
#include "rheo/domain/image_data.h"
#include "rheo/domain/terrain_data.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/descriptor.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/images.h"
#include "rheo/graphics/swap_chain.h"

namespace renderer {

class TerrainRenderer {
 public:
  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain,
            graphics::CommandPools const& command_pools,
            domain::SharedTerrain const& terrain,
            domain::SharedImage const& terrain_texture);
  void Clear(graphics::Device const& device);
  void Render(vk::raii::CommandBuffer const& command_buffer,
              graphics::SwapChain& swap_chain, renderer::Camera const& camera);

 private:
  struct CameraUBO {  // NOLINT(altera-struct-pack-align)
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    uint32_t has_terrain_texture;
  } __attribute__((aligned(16)));

  bool is_initialized_ = false;
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  graphics::AllocatedBuffer camera_ubo_buffer_;
  graphics::DescriptorAllocator descriptor_allocator_;
  vk::raii::DescriptorSetLayout descriptor_set_layout_ = nullptr;
  vk::raii::DescriptorSet descriptor_set_ = nullptr;
  std::vector<uint32_t> indices_;
  graphics::AllocatedBuffer indices_buffer_;
  graphics::AllocatedBuffer elevation_buffer_;
  graphics::AllocatedImage terrain_texture_;
  bool has_terrain_texture_ = false;

  void CreateGraphicsPipeline(graphics::Device const& device,
                              graphics::SwapChain const& swap_chain);
  void CreateDescriptorSetLayout(graphics::Device const& device);
  void CreateBuffers(graphics::Device const& device,
                     graphics::CommandPools const& command_pools,
                     std::vector<domain::Elevation> const& elevation_samples);
  void CreateImages(graphics::Device const& device,
                    graphics::CommandPools const& command_pools,
                    domain::ImageData const& terrain_texture);
  void CreateDescriptorSet(
      graphics::Device const& device,
      graphics::DescriptorAllocator const& descriptor_allocator);
  void UpdateUniformBuffer(graphics::SwapChain const& swap_chain,
                           renderer::Camera const& camera);
  void CreateIndices(uint32_t width, uint32_t height);
};

}  // namespace renderer

#endif  // !RHEOLBM_TERRAIN_RENDERER_H
