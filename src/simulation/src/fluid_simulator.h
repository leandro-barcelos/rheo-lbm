#ifndef RHEOLBM_FLUID_SIMULATOR_H
#define RHEOLBM_FLUID_SIMULATOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/domain/simulation_config.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/descriptor.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/simulation/fluid_types.h"

namespace simulation {

constexpr uint32_t kNumThreads = 256;
constexpr uint32_t kClearBucketsNumThreads = 10;

class FluidSimulator {
 public:
  // TODO: Verificar se o alinhamento foi alterado
  struct UniformBufferObject {  // NOLINT(altera-struct-pack-align)
    uint32_t voxel_max_particles;
    uint32_t fluid_particle_count;
    uint32_t wall_particle_count;
    uint32_t total_particle_count;
    float rest_density;
    float particle_mass;
    float effective_radius;
    float effective_radius_2;
    float effective_radius_6;
    float effective_radius_9;
    float viscosity;
    float gas_constant;
    float damping_coefficient;
    float mu;
    float yield_stress;
    uint32_t elevation_width;
    uint32_t elevation_height;
    uint32_t elevation_padding_3;
    glm::uvec4 bucket_size;
    glm::vec4 min_bound;
    glm::vec4 max_bound;
  } __attribute__((aligned(16)));

  struct WallParticle {
    glm::vec3 position;
  } __attribute__((aligned(16)));

  struct PushConstants {
    float time_step;
  };

  explicit FluidSimulator(domain::SimulationConfig const& parameters);

  void Init(graphics::Device const& device,
            graphics::CommandPools const& command_pools);
  [[nodiscard]] uint64_t Run(graphics::Device const& device,
                             graphics::FrameSync& frame_sync,
                             double delta_time);

  [[nodiscard]] graphics::AllocatedBuffer const& FluidParticlesReadBuffer()
      const {
    return fluid_particles_buffers_.at(read_index_);
  }
  [[nodiscard]] graphics::AllocatedBuffer const& FluidParticlesWriteBuffer()
      const {
    return fluid_particles_buffers_.at(write_index_);
  }
  [[nodiscard]] uint32_t FluidParticleCount() const {
    return uniform_buffer_data_.fluid_particle_count;
  }
  [[nodiscard]] graphics::AllocatedBuffer const& ElevationBuffer() const {
    return elevation_buffer_;
  }
  [[nodiscard]] uint32_t ElevationSampleCount() const {
    return uniform_buffer_data_.elevation_width *
           uniform_buffer_data_.elevation_height;
  }

 private:
  UniformBufferObject uniform_buffer_data_;
  std::vector<uint32_t> bucket_;
  std::vector<FluidParticle> fluid_particles_;
  std::vector<WallParticle> wall_particles_;
  graphics::AllocatedBuffer uniform_buffer_;
  std::array<graphics::AllocatedBuffer, 2> fluid_particles_buffers_;
  graphics::AllocatedBuffer wall_particles_buffer_;
  graphics::AllocatedBuffer bucket_buffer_;

  size_t read_index_ = 0;
  size_t write_index_ = 1;

  void SwapParticleBufferIndices() { std::swap(read_index_, write_index_); }
  static float DampingCoefficient(float coefficient_of_restitution);

  // Bucket Shader
  vk::raii::DescriptorSetLayout bucket_descriptor_set_layout_ = nullptr;
  vk::raii::PipelineLayout bucket_pipeline_layout_ = nullptr;
  vk::raii::Pipeline clear_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline fluid_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline wall_bucket_pipeline_ = nullptr;
  graphics::DescriptorAllocator bucket_descriptor_allocator_;
  vk::raii::DescriptorSet bucket_descriptor_set_ = nullptr;
  vk::raii::CommandBuffer clear_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer fluid_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer wall_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer bucket_primary_command_buffer_ = nullptr;

  void CreateBucketDescriptorSetLayout(graphics::Device const& device);
  void CreateBucketPipelines(graphics::Device const& device);
  void CreateBuffers(graphics::Device const& device,
                     graphics::CommandPools const& command_pools);
  void CreateFluidBucketDescriptorSets(
      graphics::Device const& device,
      graphics::DescriptorAllocator const& descriptor_allocator);
  void CreateBucketCommandBuffers(graphics::Device const& device,
                                  graphics::CommandPools const& command_pools);

  void RecordClearBucketCommandBuffer();
  void RecordFluidBucketCommandBuffer();
  void RecordWallBucketCommandBuffer();
  void RecordBucketPrimaryCommandBuffer();
  [[nodiscard]] uint64_t DispatchBucket(graphics::Device const& device,
                                        graphics::FrameSync& frame_sync,
                                        uint64_t wait_value);

  // Density Shader
  vk::raii::CommandBuffer density_command_buffer_ = nullptr;
  vk::raii::PipelineLayout density_pipeline_layout_ = nullptr;
  vk::raii::Pipeline density_pipeline_ = nullptr;
  vk::raii::DescriptorSetLayout density_descriptor_set_layout_ = nullptr;
  graphics::DescriptorAllocator density_descriptor_allocator_;
  vk::raii::DescriptorSet density_descriptor_set_ = nullptr;

  void CreateDensityDescriptorSetLayout(graphics::Device const& device);
  void CreateDensityPipeline(graphics::Device const& device);
  void CreateDensityDescriptorSets(
      graphics::Device const& device,
      graphics::DescriptorAllocator const& descriptor_allocator);
  void CreateDensityCommandBuffers(graphics::Device const& device,
                                   graphics::CommandPools const& command_pools);

  void RecordDensityCommandBuffer();
  [[nodiscard]] uint64_t DispatchDensity(graphics::Device const& device,
                                         graphics::FrameSync& frame_sync,
                                         uint64_t wait_value);

  // Vel-Pos Shader
  domain::SharedTerrain terrain_;
  graphics::AllocatedBuffer elevation_buffer_;
  PushConstants push_constants_{};
  vk::raii::CommandBuffer vel_pos_command_buffer_ = nullptr;
  vk::raii::PipelineLayout vel_pos_pipeline_layout_ = nullptr;
  vk::raii::Pipeline vel_pos_pipeline_ = nullptr;
  vk::raii::DescriptorSetLayout vel_pos_descriptor_set_layout_ = nullptr;
  graphics::DescriptorAllocator vel_pos_descriptor_allocator_;
  vk::raii::DescriptorSet vel_pos_descriptor_set_ = nullptr;

  void CreateVelPosDescriptorSetLayout(graphics::Device const& device);
  void CreateVelPosPipeline(graphics::Device const& device);
  void CreateVelPosDescriptorSets(
      graphics::Device const& device,
      graphics::DescriptorAllocator const& descriptor_allocator);
  void CreateVelPosCommandBuffers(graphics::Device const& device,
                                  graphics::CommandPools const& command_pools);

  void RecordVelPosCommandBuffer();
  [[nodiscard]] uint64_t DispatchVelPos(graphics::Device const& device,
                                        graphics::FrameSync& frame_sync,
                                        uint64_t wait_value);
};

}  // namespace simulation

static_assert(sizeof(simulation::FluidSimulator::UniformBufferObject) == 128);
static_assert(sizeof(simulation::FluidSimulator::WallParticle) == 16);

#endif  // !RHEOLBM_FLUID_SIMULATOR_H
