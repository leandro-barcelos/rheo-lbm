#define _USE_MATH_DEFINES
#include "fluid_simulator.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "rheo/graphics/pipeline.h"
#include "vulkan/vulkan.hpp"

simulation::FluidSimulator::FluidSimulator(
    domain::SimulationConfig const& parameters)
    : uniform_buffer_data_{}, terrain_(parameters.terrain) {
  const float requested_spacing = parameters.initial_particle_spacing;
  const float spacing =
      requested_spacing > 0.0F ? requested_spacing : (1.0F / 9.0F);
  const uint32_t grid_resolution =
      std::max(3U, static_cast<uint32_t>(std::floor(1.0F / spacing)) + 1U);
  const float step = 1.0F / static_cast<float>(grid_resolution - 1);

  const auto grid_resolution_size = static_cast<std::size_t>(grid_resolution);
  const auto interior_resolution_size =
      static_cast<std::size_t>(grid_resolution - 2);
  const std::size_t fluid_particle_count = interior_resolution_size *
                                           interior_resolution_size *
                                           interior_resolution_size;
  const std::size_t total_grid_particles =
      (grid_resolution_size * grid_resolution_size * grid_resolution_size);
  const std::size_t interior_grid_particles =
      (interior_resolution_size * interior_resolution_size *
       interior_resolution_size);
  const std::size_t max_wall_count =
      total_grid_particles - interior_grid_particles;
  fluid_particles_.reserve(fluid_particle_count);
  wall_particles_.reserve(max_wall_count);

  for (uint32_t z_index = 0; z_index < grid_resolution; ++z_index) {
    for (uint32_t y_index = 0; y_index < grid_resolution; ++y_index) {
      for (uint32_t x_index = 0; x_index < grid_resolution; ++x_index) {
        const glm::vec4 position{static_cast<float>(x_index) * step,
                                 static_cast<float>(y_index) * step,
                                 static_cast<float>(z_index) * step, 0.0F};
        const bool is_edge = x_index == 0 || x_index == (grid_resolution - 1) ||
                             y_index == 0 || y_index == (grid_resolution - 1) ||
                             z_index == 0 || z_index == (grid_resolution - 1);
        if (is_edge) {
          wall_particles_.emplace_back(WallParticle{.position = position});
          continue;
        }

        fluid_particles_.emplace_back(
            FluidParticle{.position = position,
                          .velocity = {0.0F, 0.0F, 0.0F, 0.0F},
                          .distance_traveled = {0.0F, 0.0F, 0.0F, 0.0F},
                          .color = {0.2F, 0.6F, 1.0F, 1.0F},
                          .density = 0.0F});
      }
    }
  }

  float total_mass = parameters.rest_density * parameters.total_fluid_volume;
  float effective_radius = step * 1.2F;
  const float simulation_extent =
      step * static_cast<float>(grid_resolution - 1);
  const glm::vec4 max_bound = {simulation_extent, simulation_extent,
                               simulation_extent, 0.0F};
  const float bucket_cell_size = std::max(effective_radius, 1e-6F);
  const uint32_t bucket_axis_size =
      static_cast<uint32_t>(std::ceil(simulation_extent / bucket_cell_size)) +
      1U;
  const glm::uvec4 bucket_size = {bucket_axis_size, bucket_axis_size,
                                  bucket_axis_size, 0U};

  uniform_buffer_data_ = {
      .voxel_max_particles = parameters.voxel_max_particles,
      .fluid_particle_count = static_cast<uint32_t>(fluid_particles_.size()),
      .wall_particle_count = static_cast<uint32_t>(wall_particles_.size()),
      .total_particle_count = static_cast<uint32_t>(fluid_particles_.size() +
                                                    wall_particles_.size()),
      .rest_density = parameters.rest_density,
      .particle_mass = total_mass / static_cast<float>(fluid_particles_.size()),
      .effective_radius = effective_radius,
      .effective_radius_2 = std::pow(effective_radius, 2.F),
      .effective_radius_6 = std::pow(effective_radius, 6.F),
      .effective_radius_9 = std::pow(effective_radius, 9.F),
      .viscosity = parameters.viscosity,
      .gas_constant = parameters.gas_constant,
      .damping_coefficient =
          DampingCoefficient(parameters.coefficient_of_restitution),
      .mu = parameters.friction,
      .yield_stress = parameters.yield_stress,
      .elevation_width = parameters.terrain->width,
      .elevation_height = parameters.terrain->height,
      .elevation_padding_3 = 0U,
      .bucket_size = bucket_size,
      .min_bound = {0.0F, 0.0F, 0.0F, 0.0F},
      .max_bound = max_bound};

  bucket_.resize(
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[0]) *
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[1]) *
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[2]) *
      static_cast<std::size_t>(uniform_buffer_data_.voxel_max_particles));
}

void simulation::FluidSimulator::Init(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  CreateBucketDescriptorSetLayout(device);
  const std::array bucket_descriptor_pool_sizes = {
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1},
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eStorageBuffer, .count = 3}};
  bucket_descriptor_allocator_.Init(device, 1, bucket_descriptor_pool_sizes);
  CreateBucketPipelines(device);
  CreateBuffers(device, command_pools);
  CreateFluidBucketDescriptorSets(device, bucket_descriptor_allocator_);
  CreateBucketCommandBuffers(device, command_pools);

  CreateDensityDescriptorSetLayout(device);
  const std::array density_descriptor_pool_sizes = {
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1},
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eStorageBuffer, .count = 3}};
  density_descriptor_allocator_.Init(device, 1, density_descriptor_pool_sizes);
  CreateDensityPipeline(device);
  CreateDensityDescriptorSets(device, density_descriptor_allocator_);
  CreateDensityCommandBuffers(device, command_pools);

  if (terrain_ == nullptr || terrain_->samples.empty()) {
    throw std::runtime_error("[ERROR] Simulation: Missing elevation samples!");
  }

  auto elevation_buffers = graphics::BufferAllocator::CreateSSBO(
      device, command_pools, terrain_->samples, false,
      vk::BufferUsageFlagBits::eVertexBuffer);
  elevation_buffer_ = std::move(elevation_buffers.front());
  CreateVelPosDescriptorSetLayout(device);
  const std::array vel_pos_descriptor_pool_sizes = {
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1},
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eStorageBuffer, .count = 4},
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eStorageBuffer, .count = 1}};
  vel_pos_descriptor_allocator_.Init(device, 1, vel_pos_descriptor_pool_sizes);
  CreateVelPosPipeline(device);
  CreateVelPosDescriptorSets(device, vel_pos_descriptor_allocator_);
  CreateVelPosCommandBuffers(device, command_pools);
}

uint64_t simulation::FluidSimulator::Run(graphics::Device const& device,
                                         graphics::FrameSync& frame_sync,
                                         double delta_time) {
  push_constants_.time_step = static_cast<float>(delta_time);
  uint64_t simulation_wait_value = frame_sync.CurrentTimelineValue();

  RecordClearBucketCommandBuffer();
  RecordFluidBucketCommandBuffer();
  RecordWallBucketCommandBuffer();
  RecordBucketPrimaryCommandBuffer();
  RecordDensityCommandBuffer();
  RecordVelPosCommandBuffer();

  uint64_t bucket_signal_value =
      DispatchBucket(device, frame_sync, simulation_wait_value);

  SwapParticleBufferIndices();

  uint64_t density_signal_value =
      DispatchDensity(device, frame_sync, bucket_signal_value);

  SwapParticleBufferIndices();

  uint64_t vel_pos_signal_value =
      DispatchVelPos(device, frame_sync, density_signal_value);

  return vel_pos_signal_value;
}

float simulation::FluidSimulator::DampingCoefficient(
    float coefficient_of_restitution) {
  float alpha_d = 0.7F;

  return static_cast<float>(
      -std::log(coefficient_of_restitution) /
      (alpha_d * std::sqrt(std::pow(std::log(coefficient_of_restitution), 2.F) +
                           std::pow(M_PI, 2.F))));
}

void simulation::FluidSimulator::CreateBucketDescriptorSetLayout(
    graphics::Device const& device) {
  std::array layout_bindings{
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr)};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
      .pBindings = layout_bindings.data()};
  bucket_descriptor_set_layout_ =
      vk::raii::DescriptorSetLayout(device.LogicalDevice(), layout_info);
}

void simulation::FluidSimulator::CreateBucketPipelines(
    graphics::Device const& device) {
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1, .pSetLayouts = &*bucket_descriptor_set_layout_};
  bucket_pipeline_layout_ =
      vk::raii::PipelineLayout(device.LogicalDevice(), pipeline_layout_info);

  fluid_bucket_pipeline_ = graphics::PipelineBuilder::Compute(
      device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "fluid_bucket");
  clear_bucket_pipeline_ = graphics::PipelineBuilder::Compute(
      device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "clear_bucket");
  wall_bucket_pipeline_ = graphics::PipelineBuilder::Compute(
      device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "wall_bucket");
}

void simulation::FluidSimulator::CreateBuffers(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  uniform_buffer_ =
      graphics::BufferAllocator::CreateUniformBuffer<UniformBufferObject>(
          device, command_pools, uniform_buffer_data_);

  fluid_particles_buffers_ = graphics::BufferAllocator::CreateSSBO(
      device, command_pools, fluid_particles_, true,
      vk::BufferUsageFlagBits::eVertexBuffer);
  wall_particles_buffer_ =
      std::move(graphics::BufferAllocator::CreateSSBO(device, command_pools,
                                                      wall_particles_, false)
                    .front());
  bucket_buffer_ = std::move(graphics::BufferAllocator::CreateSSBO(
                                 device, command_pools, bucket_, false)
                                 .front());
}

void simulation::FluidSimulator::CreateFluidBucketDescriptorSets(
    graphics::Device const& device,
    graphics::DescriptorAllocator const& descriptor_allocator) {
  bucket_descriptor_set_ =
      descriptor_allocator.Allocate(device, bucket_descriptor_set_layout_);

  vk::DescriptorBufferInfo uniform_buffer_info(uniform_buffer_.buffer, 0,
                                               sizeof(UniformBufferObject));

  vk::DescriptorBufferInfo fluid_particles_buffer_info(
      FluidParticlesReadBuffer().buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo wall_particles_buffer_info(
      wall_particles_buffer_.buffer, 0,
      sizeof(WallParticle) * uniform_buffer_data_.wall_particle_count);

  vk::DescriptorBufferInfo bucket_buffer_info(
      bucket_buffer_.buffer, 0, sizeof(uint32_t) * bucket_.size());

  std::array descriptor_writes{
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &uniform_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 2,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &wall_particles_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bucket_buffer_info,
          .pTexelBufferView = nullptr},
  };

  device.LogicalDevice().updateDescriptorSets(descriptor_writes, {});
}

void simulation::FluidSimulator::CreateBucketCommandBuffers(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo primary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  auto primary_command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), primary_allocate_info);
  bucket_primary_command_buffer_ = std::move(primary_command_buffers.front());

  vk::CommandBufferAllocateInfo secondary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::eSecondary,
      .commandBufferCount = 1};
  auto clear_command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), secondary_allocate_info);
  clear_bucket_secondary_command_buffer_ =
      std::move(clear_command_buffers.front());

  auto fluid_command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), secondary_allocate_info);
  fluid_bucket_secondary_command_buffer_ =
      std::move(fluid_command_buffers.front());

  auto wall_command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), secondary_allocate_info);
  wall_bucket_secondary_command_buffer_ =
      std::move(wall_command_buffers.front());
}

void simulation::FluidSimulator::RecordClearBucketCommandBuffer() {
  clear_bucket_secondary_command_buffer_.reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  clear_bucket_secondary_command_buffer_.begin(begin_info);

  clear_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, clear_bucket_pipeline_);
  clear_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});

  const uint32_t bucket_dispatch_x =
      (uniform_buffer_data_.bucket_size[0] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_y =
      (uniform_buffer_data_.bucket_size[1] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_z =
      (uniform_buffer_data_.bucket_size[2] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  clear_bucket_secondary_command_buffer_.dispatch(
      bucket_dispatch_x, bucket_dispatch_y, bucket_dispatch_z);

  clear_bucket_secondary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordWallBucketCommandBuffer() {
  wall_bucket_secondary_command_buffer_.reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  wall_bucket_secondary_command_buffer_.begin(begin_info);

  wall_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, wall_bucket_pipeline_);
  wall_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});
  wall_bucket_secondary_command_buffer_.dispatch(
      (uniform_buffer_data_.wall_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  wall_bucket_secondary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordFluidBucketCommandBuffer() {
  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  fluid_bucket_secondary_command_buffer_.begin(begin_info);

  fluid_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, fluid_bucket_pipeline_);
  fluid_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});
  fluid_bucket_secondary_command_buffer_.dispatch(
      (uniform_buffer_data_.fluid_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  fluid_bucket_secondary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordBucketPrimaryCommandBuffer() {
  bucket_primary_command_buffer_.reset();

  vk::CommandBufferBeginInfo begin_info{};
  bucket_primary_command_buffer_.begin(begin_info);

  std::array<vk::CommandBuffer, 3> secondary_command_buffer{
      *clear_bucket_secondary_command_buffer_,
      *fluid_bucket_secondary_command_buffer_,
      *wall_bucket_secondary_command_buffer_};
  bucket_primary_command_buffer_.executeCommands(secondary_command_buffer);

  bucket_primary_command_buffer_.end();
}

uint64_t simulation::FluidSimulator::DispatchBucket(
    graphics::Device const& device, graphics::FrameSync& frame_sync,
    uint64_t wait_value) {
  uint64_t signal_value = frame_sync.GetNextTimelineValue();

  vk::TimelineSemaphoreSubmitInfo timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo submit_info{
      .pNext = &timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*frame_sync.Semaphore(),
      .pWaitDstStageMask = wait_stages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &*bucket_primary_command_buffer_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*frame_sync.Semaphore()};

  device.ComputeQueue().submit(submit_info, nullptr);
  return signal_value;
}

void simulation::FluidSimulator::CreateDensityDescriptorSetLayout(
    graphics::Device const& device) {
  std::array layout_bindings{
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr)};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
      .pBindings = layout_bindings.data()};
  density_descriptor_set_layout_ =
      vk::raii::DescriptorSetLayout(device.LogicalDevice(), layout_info);
}

void simulation::FluidSimulator::CreateDensityPipeline(
    graphics::Device const& device) {
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1, .pSetLayouts = &*density_descriptor_set_layout_};
  density_pipeline_layout_ =
      vk::raii::PipelineLayout(device.LogicalDevice(), pipeline_layout_info);

  density_pipeline_ = graphics::PipelineBuilder::Compute(
      device, density_pipeline_layout_, "shaders/compute/density.spv");
}

void simulation::FluidSimulator::CreateDensityDescriptorSets(
    graphics::Device const& device,
    graphics::DescriptorAllocator const& descriptor_allocator) {
  density_descriptor_set_ =
      descriptor_allocator.Allocate(device, density_descriptor_set_layout_);

  vk::DescriptorBufferInfo uniform_buffer_info(uniform_buffer_.buffer, 0,
                                               sizeof(UniformBufferObject));

  vk::DescriptorBufferInfo fluid_particles_in_buffer_info(
      FluidParticlesReadBuffer().buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo fluid_particles_out_buffer_info(
      FluidParticlesWriteBuffer().buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo bucket_buffer_info(
      bucket_buffer_.buffer, 0, sizeof(uint32_t) * bucket_.size());

  std::array descriptor_writes{
      vk::WriteDescriptorSet{
          .dstSet = *density_descriptor_set_,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &uniform_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *density_descriptor_set_,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_in_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *density_descriptor_set_,
          .dstBinding = 2,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_out_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *density_descriptor_set_,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bucket_buffer_info,
          .pTexelBufferView = nullptr},
  };

  device.LogicalDevice().updateDescriptorSets(descriptor_writes, {});
}

void simulation::FluidSimulator::CreateDensityCommandBuffers(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo primary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  auto command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), primary_allocate_info);
  density_command_buffer_ = std::move(command_buffers.front());
}

void simulation::FluidSimulator::RecordDensityCommandBuffer() {
  density_command_buffer_.reset();

  vk::CommandBufferBeginInfo begin_info{};

  density_command_buffer_.begin(begin_info);

  density_command_buffer_.bindPipeline(vk::PipelineBindPoint::eCompute,
                                       density_pipeline_);
  density_command_buffer_.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             density_pipeline_layout_, 0,
                                             {density_descriptor_set_}, {});
  density_command_buffer_.dispatch(
      (uniform_buffer_data_.fluid_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  density_command_buffer_.end();
}

uint64_t simulation::FluidSimulator::DispatchDensity(
    graphics::Device const& device, graphics::FrameSync& frame_sync,
    uint64_t wait_value) {
  uint64_t signal_value = frame_sync.GetNextTimelineValue();

  vk::TimelineSemaphoreSubmitInfo timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo submit_info{.pNext = &timeline_info,
                             .waitSemaphoreCount = 1,
                             .pWaitSemaphores = &*frame_sync.Semaphore(),
                             .pWaitDstStageMask = wait_stages.data(),
                             .commandBufferCount = 1,
                             .pCommandBuffers = &*density_command_buffer_,
                             .signalSemaphoreCount = 1,
                             .pSignalSemaphores = &*frame_sync.Semaphore()};

  device.ComputeQueue().submit(submit_info, nullptr);
  return signal_value;
}

void simulation::FluidSimulator::CreateVelPosDescriptorSetLayout(
    graphics::Device const& device) {
  std::array layout_bindings{
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr)};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
      .pBindings = layout_bindings.data()};
  vel_pos_descriptor_set_layout_ =
      vk::raii::DescriptorSetLayout(device.LogicalDevice(), layout_info);
}

void simulation::FluidSimulator::CreateVelPosPipeline(
    graphics::Device const& device) {
  vk::PushConstantRange push_contant{
      .stageFlags = vk::ShaderStageFlagBits::eCompute,
      .offset = 0,
      .size = sizeof(simulation::FluidSimulator::PushConstants)};
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1,
      .pSetLayouts = &*vel_pos_descriptor_set_layout_,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_contant};
  vel_pos_pipeline_layout_ =
      vk::raii::PipelineLayout(device.LogicalDevice(), pipeline_layout_info);

  vel_pos_pipeline_ = graphics::PipelineBuilder::Compute(
      device, vel_pos_pipeline_layout_, "shaders/compute/vel_pos.spv");
}

void simulation::FluidSimulator::CreateVelPosDescriptorSets(
    graphics::Device const& device,
    graphics::DescriptorAllocator const& descriptor_allocator) {
  vel_pos_descriptor_set_ =
      descriptor_allocator.Allocate(device, vel_pos_descriptor_set_layout_);

  vk::DescriptorBufferInfo uniform_buffer_info(uniform_buffer_.buffer, 0,
                                               sizeof(UniformBufferObject));

  vk::DescriptorBufferInfo wall_particles_in_buffer_info(
      wall_particles_buffer_.buffer, 0,
      sizeof(WallParticle) * uniform_buffer_data_.wall_particle_count);

  vk::DescriptorBufferInfo fluid_particles_in_buffer_info(
      FluidParticlesReadBuffer().buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo fluid_particles_out_buffer_info(
      FluidParticlesWriteBuffer().buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo bucket_buffer_info(
      bucket_buffer_.buffer, 0, sizeof(uint32_t) * bucket_.size());

  vk::DescriptorBufferInfo elevation_buffer_info(
      elevation_buffer_.buffer, 0,
      sizeof(domain::Elevation) * terrain_->samples.size());

  std::array descriptor_writes{
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &uniform_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &wall_particles_in_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 2,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_in_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_out_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 4,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bucket_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *vel_pos_descriptor_set_,
          .dstBinding = 5,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &elevation_buffer_info,
          .pTexelBufferView = nullptr},
  };

  device.LogicalDevice().updateDescriptorSets(descriptor_writes, {});
}

void simulation::FluidSimulator::CreateVelPosCommandBuffers(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo primary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  auto command_buffers =
      vk::raii::CommandBuffers(device.LogicalDevice(), primary_allocate_info);
  vel_pos_command_buffer_ = std::move(command_buffers.front());
}

void simulation::FluidSimulator::RecordVelPosCommandBuffer() {
  vel_pos_command_buffer_.reset();

  vk::CommandBufferBeginInfo begin_info{};

  vel_pos_command_buffer_.begin(begin_info);

  vel_pos_command_buffer_.bindPipeline(vk::PipelineBindPoint::eCompute,
                                       vel_pos_pipeline_);
  vel_pos_command_buffer_.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             vel_pos_pipeline_layout_, 0,
                                             {vel_pos_descriptor_set_}, {});
  vel_pos_command_buffer_.pushConstants(
      vel_pos_pipeline_layout_, vk::ShaderStageFlagBits::eCompute, 0,
      vk::ArrayProxy<const PushConstants>(push_constants_));
  vel_pos_command_buffer_.dispatch(
      (uniform_buffer_data_.fluid_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  vel_pos_command_buffer_.end();
}

[[nodiscard]] uint64_t simulation::FluidSimulator::DispatchVelPos(
    graphics::Device const& device, graphics::FrameSync& frame_sync,
    uint64_t wait_value) {
  uint64_t signal_value = frame_sync.GetNextTimelineValue();

  vk::TimelineSemaphoreSubmitInfo timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo submit_info{.pNext = &timeline_info,
                             .waitSemaphoreCount = 1,
                             .pWaitSemaphores = &*frame_sync.Semaphore(),
                             .pWaitDstStageMask = wait_stages.data(),
                             .commandBufferCount = 1,
                             .pCommandBuffers = &*vel_pos_command_buffer_,
                             .signalSemaphoreCount = 1,
                             .pSignalSemaphores = &*frame_sync.Semaphore()};

  device.ComputeQueue().submit(submit_info, nullptr);
  return signal_value;
}
