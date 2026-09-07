#ifndef RHEOLBM_PIPELINE_H
#define RHEOLBM_PIPELINE_H

#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/device.h"
#include "rheo/graphics/swap_chain.h"

namespace graphics {

class PipelineBuilder {
 public:
  struct GraphicsOptions {
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::ePointList;
    vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
    vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
    vk::FrontFace front_face = vk::FrontFace::eCounterClockwise;
    bool enable_blending = true;
    bool depth_test_enable = false;
    bool depth_write_enable = false;
    bool vertex_pulling = false;
    vk::Format depth_format = vk::Format::eUndefined;
  } __attribute__((aligned(32)));

  [[nodiscard]] static vk::raii::Pipeline Compute(
      graphics::Device const& device,
      vk::raii::PipelineLayout const& pipeline_layout,
      std::string const& shader_filename, const char* name = "comp_main");
  [[nodiscard]] static vk::raii::Pipeline Graphics(
      graphics::Device const& device,
      vk::VertexInputBindingDescription binding_description,
      std::vector<vk::VertexInputAttributeDescription> const&
          attribute_descriptions,
      vk::raii::PipelineLayout const& pipeline_layout,
      graphics::SwapChain const& swap_chain,
      std::string const& shader_filename);
  [[nodiscard]] static vk::raii::Pipeline Graphics(
      graphics::Device const& device,
      vk::VertexInputBindingDescription binding_description,
      std::vector<vk::VertexInputAttributeDescription> const&
          attribute_descriptions,
      vk::raii::PipelineLayout const& pipeline_layout,
      graphics::SwapChain const& swap_chain, std::string const& shader_filename,
      GraphicsOptions options);
};

}  // namespace graphics

#endif  // !RHEOLBM_PIPELINE_H
