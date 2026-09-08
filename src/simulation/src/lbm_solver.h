#ifndef RHEO_SIMULATION_LBM_SOLVER_H
#define RHEO_SIMULATION_LBM_SOLVER_H

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/domain/lattice_definition.h"
#include "rheo/domain/simulation_config.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/frame_sync.h"

namespace simulation {

class LbmSolver {
 public:
  struct DebugBuffers {
    std::array<graphics::AllocatedBuffer const*, 2> momentum;
    std::uint32_t read_index;
  };

  LbmSolver() = default;
  void Initialize(graphics::Device const& device,
                  graphics::CommandPools const& pools,
                  graphics::FrameSync& sync,
                  graphics::AllocatedBuffer const& lattice,
                  domain::LatticeDefinition const& definition,
                  domain::LbmSettings const& settings,
                  std::uint64_t wait_signal);
  std::uint64_t Step(graphics::Device const& device, graphics::FrameSync& sync,
                     graphics::AllocatedBuffer const& lattice,
                     domain::LbmSettings const& settings,
                     std::uint64_t wait_signal);
  std::uint64_t RemoveDam(graphics::Device const& device,
                          graphics::FrameSync& sync,
                          domain::LbmSettings const& settings,
                          std::uint64_t wait_signal);
  [[nodiscard]] DebugBuffers Buffers() const {
    return {{&momentum_[0], &momentum_[1]}, read_index_};
  }

 private:
  struct Parameters {
    glm::uvec4 shape{};
    glm::vec4 fluid{};
    glm::vec4 interface_values{};
    glm::vec4 gravity{};
  };
  void Record(vk::raii::CommandBuffer const& command,
              domain::LbmSettings const& settings, bool initialize);
  void Bind(vk::raii::CommandBuffer const& command,
            vk::raii::Pipeline const& pipeline,
            std::uint32_t descriptor_index) const;
  static void Barrier(vk::raii::CommandBuffer const& command);
  std::uint64_t Submit(graphics::Device const& device,
                       graphics::FrameSync& sync,
                       vk::raii::CommandBuffer const& command,
                       std::uint64_t wait_signal);

  domain::LatticeDefinition definition_{};
  std::array<graphics::AllocatedBuffer, 2> momentum_;
  graphics::AllocatedBuffer after_collision_, next_mass_, transition_mask_,
      excess_;
  vk::raii::DescriptorSetLayout descriptor_layout_ = nullptr;
  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  std::array<vk::raii::DescriptorSet, 2> descriptors_{nullptr, nullptr};
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;
  vk::raii::Pipeline initialize_ = nullptr, collide_ = nullptr,
                     calculate_streaming_ = nullptr, apply_streaming_ = nullptr,
                     mark_transitions_ = nullptr,
                     update_fluid_neighbors_ = nullptr,
                     apply_gas_to_interface_ = nullptr,
                     update_gas_neighbors_ = nullptr,
                     calculate_excess_ = nullptr, apply_excess_ = nullptr,
                     apply_transitions_ = nullptr, remove_dam_ = nullptr,
                     reclassify_after_dam_ = nullptr;
  vk::raii::CommandBuffer initialize_command_ = nullptr,
                          step_command_ = nullptr, dam_command_ = nullptr;
  std::uint32_t read_index_ = 0;
};

}  // namespace simulation
#endif
