#include "lattice_editor.h"

#include <array>

#include "rheo/graphics/pipeline.h"
#include "rheo/simulation/simulation_types.h"
namespace simulation {
std::uint64_t LatticeEditor::Apply(graphics::Device const& device,
                                   graphics::CommandPools const& pools,
                                   graphics::FrameSync& sync,
                                   graphics::AllocatedBuffer const& lattice,
                                   std::uint32_t count,
                                   std::span<domain::CellDelta const> changes) {
  if (changes.empty()) return sync.CurrentTimelineValue();
  if (!*pipeline_) {
    auto limits = device.PhysicalDevice().getProperties().limits;
    if (limits.maxComputeWorkGroupInvocations < 256 ||
        limits.maxComputeWorkGroupSize[0] < 256)
      throw std::runtime_error(
          "GPU does not support the lattice editor workgroup");
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
    layout_ = vk::raii::DescriptorSetLayout(
        device.LogicalDevice(),
        {.bindingCount = 2, .pBindings = bindings.data()});
    vk::DescriptorPoolSize size{.type = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 2};
    pool_ = vk::raii::DescriptorPool(
        device.LogicalDevice(),
        {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
         .maxSets = 1,
         .poolSizeCount = 1,
         .pPoolSizes = &size});
    descriptor_ =
        std::move(device.LogicalDevice()
                      .allocateDescriptorSets({.descriptorPool = *pool_,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &*layout_})
                      .front());
    vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                .size = 8};
    pipeline_layout_ = vk::raii::PipelineLayout(
        device.LogicalDevice(), {.setLayoutCount = 1,
                                 .pSetLayouts = &*layout_,
                                 .pushConstantRangeCount = 1,
                                 .pPushConstantRanges = &range});
    pipeline_ = graphics::PipelineBuilder::Compute(
        device, pipeline_layout_, "shaders/compute/lattice_editor.spv",
        "edit_cells");
    command_ = std::move(
        device.LogicalDevice()
            .allocateCommandBuffers({.commandPool = *pools.Compute(),
                                     .level = vk::CommandBufferLevel::ePrimary,
                                     .commandBufferCount = 1})
            .front());
  }
  if (changes.size() > capacity_) {
    auto next = graphics::BufferAllocator::CreateBuffer(
        device, changes.size() * 8, vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    changes_ = std::move(next);
    capacity_ = changes.size();
  }
  auto* mapped = static_cast<glm::uvec2*>(changes_.Mapped(changes.size() * 8));
  for (std::size_t i = 0; i < changes.size(); ++i)
    mapped[i] = {changes[i].index, changes[i].after};
  changes_.Flush(changes.size() * 8);
  std::array infos = {
      vk::DescriptorBufferInfo{.buffer = *changes_.Buffer(),
                               .offset = 0,
                               .range = changes.size() * 8},
      vk::DescriptorBufferInfo{.buffer = *lattice.Buffer(),
                               .offset = 0,
                               .range = vk::DeviceSize(count) * sizeof(Cell)}};
  std::array writes = {vk::WriteDescriptorSet{
                           .dstSet = *descriptor_,
                           .dstBinding = 0,
                           .descriptorCount = 1,
                           .descriptorType = vk::DescriptorType::eStorageBuffer,
                           .pBufferInfo = &infos[0]},
                       vk::WriteDescriptorSet{
                           .dstSet = *descriptor_,
                           .dstBinding = 1,
                           .descriptorCount = 1,
                           .descriptorType = vk::DescriptorType::eStorageBuffer,
                           .pBufferInfo = &infos[1]}};
  device.LogicalDevice().updateDescriptorSets(writes, {});
  command_.reset();
  command_.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command_.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline_);
  command_.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              *pipeline_layout_, 0, {*descriptor_}, {});
  auto max_batch = std::uint64_t(device.PhysicalDevice()
                                     .getProperties()
                                     .limits.maxComputeWorkGroupCount[0]) *
                   256;
  for (std::uint64_t offset = 0; offset < changes.size();) {
    auto batch = std::uint32_t(
        std::min<std::uint64_t>(changes.size() - offset, max_batch));
    glm::uvec2 parameters{batch, offset};
    command_.pushConstants<glm::uvec2>(
        *pipeline_layout_, vk::ShaderStageFlagBits::eCompute, 0, parameters);
    command_.dispatch((batch + 255ULL) / 256, 1, 1);
    offset += batch;
  }
  command_.end();
  auto wait = sync.CurrentTimelineValue(), signal = sync.GetNextTimelineValue();
  vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eComputeShader;
  vk::TimelineSemaphoreSubmitInfo timeline{
      .waitSemaphoreValueCount = wait ? 1U : 0U,
      .pWaitSemaphoreValues = &wait,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal};
  device.ComputeQueue().submit(
      vk::SubmitInfo{.pNext = &timeline,
                     .waitSemaphoreCount = wait ? 1U : 0U,
                     .pWaitSemaphores = &*sync.Semaphore(),
                     .pWaitDstStageMask = &stage,
                     .commandBufferCount = 1,
                     .pCommandBuffers = &*command_,
                     .signalSemaphoreCount = 1,
                     .pSignalSemaphores = &*sync.Semaphore()},
      nullptr);
  sync.WaitSemaphore(device, signal);
  return signal;
}
}  // namespace simulation
