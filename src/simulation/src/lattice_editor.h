#ifndef RHEO_SIMULATION_LATTICE_EDITOR_H
#define RHEO_SIMULATION_LATTICE_EDITOR_H
#include "rheo/domain/lattice_editing.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/frame_sync.h"
namespace simulation {
class LatticeEditor {
 public:
  std::uint64_t Apply(graphics::Device const& device,
                      graphics::CommandPools const& pools,
                      graphics::FrameSync& sync,
                      graphics::AllocatedBuffer const& lattice,
                      std::uint32_t count,
                      std::span<domain::CellDelta const> changes);

 private:
  graphics::AllocatedBuffer changes_;
  std::size_t capacity_ = 0;
  vk::raii::DescriptorSetLayout layout_ = nullptr;
  vk::raii::DescriptorPool pool_ = nullptr;
  vk::raii::DescriptorSet descriptor_ = nullptr;
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;
  vk::raii::Pipeline pipeline_ = nullptr;
  vk::raii::CommandBuffer command_ = nullptr;
};
}  // namespace simulation
#endif
