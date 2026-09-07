#include "lattice_initializer.h"

#include <array>
#include <cstring>
#include <stdexcept>

#include "rheo/graphics/pipeline.h"
#include "rheo/simulation/simulation_types.h"
namespace simulation {
graphics::AllocatedBuffer LatticeInitializer::Build(
    graphics::Device const& device, graphics::CommandPools const& pools,
    graphics::FrameSync& sync, domain::DemData const& dem,
    domain::LatticeDefinition const& definition, std::uint64_t& ready_signal) {
  auto const limits = device.PhysicalDevice().getProperties().limits;
  auto const bytes =
      static_cast<vk::DeviceSize>(definition.cell_count) * sizeof(Cell);
  auto const elevation_bytes =
      static_cast<vk::DeviceSize>(dem.samples.size()) * sizeof(float);
  std::array<std::uint32_t, 3> const groups = {
      static_cast<std::uint32_t>((definition.width + 3ULL) / 4),
      static_cast<std::uint32_t>((definition.height + 3ULL) / 4),
      static_cast<std::uint32_t>((definition.depth + 3ULL) / 4)};
  if (bytes > limits.maxStorageBufferRange ||
      elevation_bytes > limits.maxStorageBufferRange)
    throw std::runtime_error(
        "Lattice or DEM exceeds GPU storage buffer limit; reduce height "
        "subdivisions");
  for (int i = 0; i < 3; ++i)
    if (groups[i] > limits.maxComputeWorkGroupCount[i] ||
        limits.maxComputeWorkGroupSize[i] < 4)
      throw std::runtime_error("Lattice exceeds GPU dispatch limits");
  if (limits.maxComputeWorkGroupInvocations < 64)
    throw std::runtime_error("GPU does not support the voxelizer workgroup");
  auto elevations = graphics::BufferAllocator::CreateBuffer(
      device, elevation_bytes, vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);
  auto* mapped =
      static_cast<float*>(elevations.memory.mapMemory(0, elevation_bytes));
  for (std::size_t i = 0; i < dem.samples.size(); ++i)
    mapped[i] = dem.samples[i].elevation;
  elevations.memory.unmapMemory();
  auto lattice = graphics::BufferAllocator::CreateBuffer(
      device, bytes,
      vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  std::array bindings = {
      vk::DescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
      vk::DescriptorSetLayoutBinding{
          .binding = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute}};
  vk::raii::DescriptorSetLayout layout(
      device.LogicalDevice(),
      {.bindingCount = 2, .pBindings = bindings.data()});
  struct Parameters {
    glm::uvec4 shape;
    glm::uvec2 elevation_shape;
    float min_elevation, meters_per_cell;
  };
  static_assert(sizeof(Parameters) == 32);
  Parameters parameters{
      {definition.width, definition.height, definition.depth, 0},
      {dem.width, dem.height},
      dem.min_elevation,
      definition.meters_per_cell};
  vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                              .offset = 0,
                              .size = sizeof(Parameters)};
  vk::raii::PipelineLayout pipeline_layout(device.LogicalDevice(),
                                           {.setLayoutCount = 1,
                                            .pSetLayouts = &*layout,
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = &range});
  auto pipeline = graphics::PipelineBuilder::Compute(
      device, pipeline_layout, "shaders/compute/voxelize_terrain.spv",
      "voxelize_terrain");
  vk::DescriptorPoolSize pool_size{.type = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 2};
  vk::raii::DescriptorPool pool(
      device.LogicalDevice(),
      {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
       .maxSets = 1,
       .poolSizeCount = 1,
       .pPoolSizes = &pool_size});
  auto descriptor =
      std::move(device.LogicalDevice()
                    .allocateDescriptorSets({.descriptorPool = *pool,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &*layout})
                    .front());
  std::array infos = {
      vk::DescriptorBufferInfo{
          .buffer = *elevations.buffer, .offset = 0, .range = elevation_bytes},
      vk::DescriptorBufferInfo{
          .buffer = *lattice.buffer, .offset = 0, .range = bytes}};
  std::array writes = {vk::WriteDescriptorSet{
                           .dstSet = *descriptor,
                           .dstBinding = 0,
                           .descriptorCount = 1,
                           .descriptorType = vk::DescriptorType::eStorageBuffer,
                           .pBufferInfo = &infos[0]},
                       vk::WriteDescriptorSet{
                           .dstSet = *descriptor,
                           .dstBinding = 1,
                           .descriptorCount = 1,
                           .descriptorType = vk::DescriptorType::eStorageBuffer,
                           .pBufferInfo = &infos[1]}};
  device.LogicalDevice().updateDescriptorSets(writes, {});
  auto command = std::move(
      device.LogicalDevice()
          .allocateCommandBuffers({.commandPool = *pools.Compute(),
                                   .level = vk::CommandBufferLevel::ePrimary,
                                   .commandBufferCount = 1})
          .front());
  command.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
  command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline_layout,
                             0, {*descriptor}, {});
  command.pushConstants<Parameters>(
      *pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, parameters);
  command.dispatch(groups[0], groups[1], groups[2]);
  command.end();
  // Submission makes coherent host writes visible. The timeline signal makes
  // compute writes visible to graphics (including a separate queue family).
  ready_signal = sync.GetNextTimelineValue();
  vk::TimelineSemaphoreSubmitInfo timeline{
      .signalSemaphoreValueCount = 1, .pSignalSemaphoreValues = &ready_signal};
  vk::SubmitInfo submit{.pNext = &timeline,
                        .commandBufferCount = 1,
                        .pCommandBuffers = &*command,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = &*sync.Semaphore()};
  device.ComputeQueue().submit(submit, nullptr);
  sync.WaitSemaphore(device, ready_signal);
  return lattice;
}
}  // namespace simulation
