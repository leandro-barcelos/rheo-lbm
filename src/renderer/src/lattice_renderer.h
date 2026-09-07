#ifndef RHEO_LATTICE_RENDERER_H
#define RHEO_LATTICE_RENDERER_H
#include <optional>

#include "camera.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/simulation/simulation_types.h"
namespace renderer {
class LatticeRenderer {
 public:
  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain, vk::Format depth_format);
  void Shutdown();
  void Render(vk::raii::CommandBuffer const& command,
              graphics::SwapChain const& swap_chain,
              std::optional<simulation::LatticeRenderSnapshot> const& lattice,
              Camera const& camera);

 private:
  struct alignas(16) CameraUBO {
    glm::mat4 model, view, proj;
  };
  graphics::Device const* device_ = nullptr;
  graphics::AllocatedBuffer camera_buffer_;
  vk::raii::DescriptorSetLayout descriptor_layout_ = nullptr;
  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  vk::raii::DescriptorSet descriptor_ = nullptr;
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;
  vk::raii::Pipeline pipeline_ = nullptr;
  std::uint64_t loaded_signal_ = 0;
  std::uintptr_t loaded_buffer_ = 0;
};
}  // namespace renderer
#endif
