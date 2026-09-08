#include "lbm_solver.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include "rheo/graphics/pipeline.h"
#include "rheo/simulation/simulation_types.h"

namespace simulation {
namespace {
constexpr vk::DeviceSize kDirections = 19;
constexpr std::uint32_t kBindingCount = 7;
}  // namespace

void LbmSolver::Initialize(graphics::Device const& device,
                           graphics::CommandPools const& pools,
                           graphics::FrameSync& sync,
                           graphics::AllocatedBuffer const& lattice,
                           domain::LatticeDefinition const& definition,
                           domain::LbmSettings const& settings,
                           std::uint64_t wait_signal) {
  static_assert(sizeof(Parameters) == 64);
  auto errors = domain::ValidateLbmSettings(settings);
  if (!errors.empty()) throw std::runtime_error(errors.front());
  auto const limits = device.PhysicalDevice().getProperties().limits;
  if (limits.maxComputeWorkGroupInvocations < 128 ||
      limits.maxComputeWorkGroupSize[0] < 64 ||
      limits.maxComputeWorkGroupSize[1] < 2)
    throw std::runtime_error("GPU does not support the LBM workgroup size");
  auto count = vk::DeviceSize(definition.cell_count);
  if (count > std::numeric_limits<vk::DeviceSize>::max() /
                  (kDirections * sizeof(float)))
    throw std::runtime_error("LBM buffer size overflow");
  vk::DeviceSize distribution_bytes = count * kDirections * sizeof(float);
  vk::DeviceSize scalar_bytes = count * sizeof(float);
  if (distribution_bytes > limits.maxStorageBufferRange ||
      scalar_bytes > limits.maxStorageBufferRange ||
      count * sizeof(Cell) > limits.maxStorageBufferRange)
    throw std::runtime_error("Lattice exceeds the GPU storage-buffer range");
  glm::uvec3 groups{(definition.width + 63U) / 64U,
                    (definition.height + 1U) / 2U, definition.depth};
  for (int axis = 0; axis < 3; ++axis)
    if (groups[axis] > limits.maxComputeWorkGroupCount[axis])
      throw std::runtime_error("Lattice exceeds the GPU dispatch limits");

  definition_ = definition;
  for (auto& buffer : momentum_)
    buffer = graphics::BufferAllocator::CreateBuffer(
        device, distribution_bytes,
        vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
  after_collision_ = graphics::BufferAllocator::CreateBuffer(
      device, distribution_bytes,
      vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  excess_ = graphics::BufferAllocator::CreateBuffer(
      device, distribution_bytes, vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  next_mass_ = graphics::BufferAllocator::CreateBuffer(
      device, scalar_bytes, vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  transition_mask_ = graphics::BufferAllocator::CreateBuffer(
      device, count * sizeof(std::uint32_t),
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eDeviceLocal);

  std::array<vk::DescriptorSetLayoutBinding, kBindingCount> bindings{};
  for (std::uint32_t i = 0; i < bindings.size(); ++i)
    bindings[i] = {.binding = i,
                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                   .descriptorCount = 1,
                   .stageFlags = vk::ShaderStageFlagBits::eCompute};
  descriptor_layout_ = vk::raii::DescriptorSetLayout(
      device.LogicalDevice(),
      {.bindingCount = kBindingCount, .pBindings = bindings.data()});
  vk::DescriptorPoolSize pool_size{.type = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 2 * kBindingCount};
  descriptor_pool_ = vk::raii::DescriptorPool(
      device.LogicalDevice(),
      {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
       .maxSets = 2,
       .poolSizeCount = 1,
       .pPoolSizes = &pool_size});
  std::array layouts = {*descriptor_layout_, *descriptor_layout_};
  auto sets = device.LogicalDevice().allocateDescriptorSets(
      {.descriptorPool = *descriptor_pool_,
       .descriptorSetCount = 2,
       .pSetLayouts = layouts.data()});
  descriptors_[0] = std::move(sets[0]);
  descriptors_[1] = std::move(sets[1]);

  for (std::uint32_t read = 0; read < 2; ++read) {
    std::array infos = {
        vk::DescriptorBufferInfo{*lattice.buffer, 0, count * sizeof(Cell)},
        vk::DescriptorBufferInfo{*momentum_[read].buffer, 0,
                                 distribution_bytes},
        vk::DescriptorBufferInfo{*momentum_[1U - read].buffer, 0,
                                 distribution_bytes},
        vk::DescriptorBufferInfo{*after_collision_.buffer, 0,
                                 distribution_bytes},
        vk::DescriptorBufferInfo{*next_mass_.buffer, 0, scalar_bytes},
        vk::DescriptorBufferInfo{*transition_mask_.buffer, 0,
                                 count * sizeof(std::uint32_t)},
        vk::DescriptorBufferInfo{*excess_.buffer, 0, distribution_bytes}};
    std::array<vk::WriteDescriptorSet, kBindingCount> writes{};
    for (std::uint32_t binding = 0; binding < kBindingCount; ++binding)
      writes[binding] = {.dstSet = *descriptors_[read],
                         .dstBinding = binding,
                         .descriptorCount = 1,
                         .descriptorType = vk::DescriptorType::eStorageBuffer,
                         .pBufferInfo = &infos[binding]};
    device.LogicalDevice().updateDescriptorSets(writes, {});
  }

  vk::PushConstantRange push{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                             .size = sizeof(Parameters)};
  pipeline_layout_ = vk::raii::PipelineLayout(
      device.LogicalDevice(), {.setLayoutCount = 1,
                               .pSetLayouts = &*descriptor_layout_,
                               .pushConstantRangeCount = 1,
                               .pPushConstantRanges = &push});
  auto make = [&](char const* file, char const* entry) {
    return graphics::PipelineBuilder::Compute(device, pipeline_layout_, file,
                                              entry);
  };
  initialize_ = make("shaders/compute/collision.spv", "initialize_lbm");
  collide_ = make("shaders/compute/collision.spv", "collide");
  calculate_streaming_ =
      make("shaders/compute/streaming.spv", "calculate_streaming");
  apply_streaming_ = make("shaders/compute/streaming.spv", "apply_streaming");
  mark_transitions_ =
      make("shaders/compute/transitions.spv", "mark_transitions");
  update_fluid_neighbors_ = make("shaders/compute/transitions.spv",
                                 "update_fluid_transition_neighbors");
  apply_gas_to_interface_ =
      make("shaders/compute/transitions.spv", "apply_gas_to_interface");
  update_gas_neighbors_ = make("shaders/compute/transitions.spv",
                               "update_gas_transition_neighbors");
  calculate_excess_ =
      make("shaders/compute/transitions.spv", "calculate_excess_distribution");
  apply_excess_ =
      make("shaders/compute/transitions.spv", "apply_excess_distribution");
  apply_transitions_ =
      make("shaders/compute/transitions.spv", "apply_transitions");
  remove_dam_ = make("shaders/compute/transitions.spv", "remove_dam");
  reclassify_after_dam_ =
      make("shaders/compute/transitions.spv", "reclassify_after_dam_removal");
  auto commands = device.LogicalDevice().allocateCommandBuffers(
      {.commandPool = *pools.Compute(),
       .level = vk::CommandBufferLevel::ePrimary,
       .commandBufferCount = 3});
  initialize_command_ = std::move(commands[0]);
  step_command_ = std::move(commands[1]);
  dam_command_ = std::move(commands[2]);
  read_index_ = 0;
  Record(initialize_command_, settings, true);
  auto signal = Submit(device, sync, initialize_command_, wait_signal);
  sync.WaitSemaphore(device, signal);
}

void LbmSolver::Bind(vk::raii::CommandBuffer const& command,
                     vk::raii::Pipeline const& pipeline,
                     std::uint32_t descriptor_index) const {
  command.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
  command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline_layout_,
                             0, {*descriptors_[descriptor_index]}, {});
}

void LbmSolver::Barrier(vk::raii::CommandBuffer const& command) {
  vk::MemoryBarrier2 barrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask =
          vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite};
  command.pipelineBarrier2(
      {.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});
}

void LbmSolver::Record(vk::raii::CommandBuffer const& command,
                       domain::LbmSettings const& settings, bool initialize) {
  Parameters parameters{
      .shape = {definition_.width, definition_.height, definition_.depth, 0},
      .fluid = {settings.initial_density, settings.omega, settings.max_velocity,
                settings.atmospheric_density},
      .interface_values = {settings.fill_offset, settings.lonely_threshold, 0,
                           0},
      .gravity = {settings.gravity, 0}};
  glm::uvec3 groups{(definition_.width + 63U) / 64U,
                    (definition_.height + 1U) / 2U, definition_.depth};
  command.reset();
  command.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command.pushConstants<Parameters>(
      *pipeline_layout_, vk::ShaderStageFlagBits::eCompute, 0, parameters);
  if (initialize) {
    Bind(command, initialize_, 0);
    command.dispatch(groups.x, groups.y, groups.z);
    command.end();
    return;
  }
  auto dispatch = [&](vk::raii::Pipeline const& pipeline,
                      std::uint32_t descriptor) {
    Bind(command, pipeline, descriptor);
    command.dispatch(groups.x, groups.y, groups.z);
    Barrier(command);
  };
  std::uint32_t const write_index = 1U - read_index_;
  dispatch(collide_, read_index_);
  dispatch(calculate_streaming_, read_index_);
  dispatch(apply_streaming_, write_index);
  dispatch(mark_transitions_, write_index);
  dispatch(update_fluid_neighbors_, write_index);
  dispatch(apply_gas_to_interface_, write_index);
  dispatch(update_gas_neighbors_, write_index);
  dispatch(calculate_excess_, write_index);
  dispatch(apply_excess_, write_index);
  Bind(command, apply_transitions_, write_index);
  command.dispatch(groups.x, groups.y, groups.z);
  command.end();
}

std::uint64_t LbmSolver::Submit(graphics::Device const& device,
                                graphics::FrameSync& sync,
                                vk::raii::CommandBuffer const& command,
                                std::uint64_t wait_signal) {
  auto signal = sync.GetNextTimelineValue();
  vk::TimelineSemaphoreSubmitInfo timeline{
      .waitSemaphoreValueCount = wait_signal ? 1U : 0U,
      .pWaitSemaphoreValues = wait_signal ? &wait_signal : nullptr,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal};
  vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eComputeShader;
  device.ComputeQueue().submit(
      vk::SubmitInfo{
          .pNext = &timeline,
          .waitSemaphoreCount = wait_signal ? 1U : 0U,
          .pWaitSemaphores = wait_signal ? &*sync.Semaphore() : nullptr,
          .pWaitDstStageMask = wait_signal ? &stage : nullptr,
          .commandBufferCount = 1,
          .pCommandBuffers = &*command,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &*sync.Semaphore()},
      nullptr);
  return signal;
}

std::uint64_t LbmSolver::Step(graphics::Device const& device,
                              graphics::FrameSync& sync,
                              graphics::AllocatedBuffer const& lattice,
                              domain::LbmSettings const& settings,
                              std::uint64_t wait_signal) {
  (void)lattice;
  // The command buffer is reused. Waiting for the latest shared timeline
  // value also covers a graphics read of the lattice on the other queue.
  auto const safe_signal = sync.CurrentTimelineValue();
  if (safe_signal != 0) sync.WaitSemaphore(device, safe_signal);
  Record(step_command_, settings, false);
  auto signal =
      Submit(device, sync, step_command_, std::max(wait_signal, safe_signal));
  read_index_ = 1U - read_index_;
  return signal;
}

std::uint64_t LbmSolver::RemoveDam(graphics::Device const& device,
                                   graphics::FrameSync& sync,
                                   domain::LbmSettings const& settings,
                                   std::uint64_t wait_signal) {
  auto const safe_signal = sync.CurrentTimelineValue();
  if (safe_signal != 0) sync.WaitSemaphore(device, safe_signal);
  Parameters parameters{
      .shape = {definition_.width, definition_.height, definition_.depth, 0},
      .fluid = {settings.initial_density, settings.omega, settings.max_velocity,
                settings.atmospheric_density},
      .interface_values = {settings.fill_offset, settings.lonely_threshold, 0,
                           0},
      .gravity = {settings.gravity, 0}};
  glm::uvec3 groups{(definition_.width + 63U) / 64U,
                    (definition_.height + 1U) / 2U, definition_.depth};
  dam_command_.reset();
  dam_command_.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  dam_command_.pushConstants<Parameters>(
      *pipeline_layout_, vk::ShaderStageFlagBits::eCompute, 0, parameters);
  Bind(dam_command_, remove_dam_, read_index_);
  dam_command_.dispatch(groups.x, groups.y, groups.z);
  Barrier(dam_command_);
  Bind(dam_command_, reclassify_after_dam_, read_index_);
  dam_command_.dispatch(groups.x, groups.y, groups.z);
  dam_command_.end();
  return Submit(device, sync, dam_command_, std::max(wait_signal, safe_signal));
}
}  // namespace simulation
