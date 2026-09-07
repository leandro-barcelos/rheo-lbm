#include "lattice_renderer.h"

#include <array>

#include "rheo/graphics/pipeline.h"
namespace renderer {
void LatticeRenderer::Init(graphics::Device const& device,
                           graphics::SwapChain const& swap_chain,
                           vk::Format depth_format) {
  device_ = &device;
  std::array bindings = {
      vk::DescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex},
      vk::DescriptorSetLayoutBinding{
          .binding = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex}};
  descriptor_layout_ = vk::raii::DescriptorSetLayout(
      device.LogicalDevice(),
      {.bindingCount = 2, .pBindings = bindings.data()});
  std::array sizes = {
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                             .descriptorCount = 1},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                             .descriptorCount = 1}};
  descriptor_pool_ = vk::raii::DescriptorPool(
      device.LogicalDevice(),
      {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
       .maxSets = 1,
       .poolSizeCount = 2,
       .pPoolSizes = sizes.data()});
  descriptor_ = std::move(
      device.LogicalDevice()
          .allocateDescriptorSets({.descriptorPool = *descriptor_pool_,
                                   .descriptorSetCount = 1,
                                   .pSetLayouts = &*descriptor_layout_})
          .front());
  camera_buffer_ = graphics::BufferAllocator::CreateMappedUniformBuffer(
      device, sizeof(CameraUBO));
  vk::DescriptorBufferInfo info{.buffer = *camera_buffer_.buffer,
                                .offset = 0,
                                .range = sizeof(CameraUBO)};
  vk::WriteDescriptorSet write{
      .dstSet = *descriptor_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .pBufferInfo = &info};
  device.LogicalDevice().updateDescriptorSets(write, {});
  vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eVertex,
                              .offset = 0,
                              .size = sizeof(glm::vec4)};
  pipeline_layout_ = vk::raii::PipelineLayout(
      device.LogicalDevice(), {.setLayoutCount = 1,
                               .pSetLayouts = &*descriptor_layout_,
                               .pushConstantRangeCount = 1,
                               .pPushConstantRanges = &range});
  pipeline_ = graphics::PipelineBuilder::Graphics(
      device, {}, {}, pipeline_layout_, swap_chain,
      "shaders/graphics/lattice.spv",
      {.topology = vk::PrimitiveTopology::eTriangleList,
       .cull_mode = vk::CullModeFlagBits::eNone,
       .enable_blending = false,
       .depth_test_enable = true,
       .depth_write_enable = true,
       .vertex_pulling = true,
       .depth_format = depth_format});
}
void LatticeRenderer::Shutdown() {
  pipeline_ = nullptr;
  pipeline_layout_ = nullptr;
  descriptor_ = nullptr;
  descriptor_pool_ = nullptr;
  descriptor_layout_ = nullptr;
  camera_buffer_ = {};
  loaded_signal_ = 0;
  loaded_buffer_ = 0;
  device_ = nullptr;
}

void LatticeRenderer::Render(
    vk::raii::CommandBuffer const& command,
    graphics::SwapChain const& swap_chain,
    std::optional<simulation::LatticeRenderSnapshot> const& lattice,
    Camera const& camera) {
  if (!lattice || !lattice->cell_count) return;
  CameraUBO ubo{glm::mat4(1), camera.ViewMatrix(),
                camera.ProjectionMatrix(float(swap_chain.Extent().width) /
                                        swap_chain.Extent().height)};
  graphics::BufferAllocator::WriteMapped(camera_buffer_, &ubo, sizeof(ubo));
  if (loaded_signal_ != lattice->ready_signal ||
      loaded_buffer_ != lattice->lattice_buffer.native_handle) {
    vk::DescriptorBufferInfo info{
        .buffer = vk::Buffer(
            reinterpret_cast<VkBuffer>(lattice->lattice_buffer.native_handle)),
        .offset = 0,
        .range =
            vk::DeviceSize(lattice->cell_count) * sizeof(simulation::Cell)};
    vk::WriteDescriptorSet write{
        .dstSet = *descriptor_,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = &info};
    device_->LogicalDevice().updateDescriptorSets(write, {});
    loaded_signal_ = lattice->ready_signal;
    loaded_buffer_ = lattice->lattice_buffer.native_handle;
  }
  command.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
  command.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                             *pipeline_layout_, 0, {*descriptor_}, {});
  command.setViewport(0, vk::Viewport(0, 0, float(swap_chain.Extent().width),
                                      float(swap_chain.Extent().height), 0, 1));
  command.setScissor(0, vk::Rect2D({0, 0}, swap_chain.Extent()));
  glm::vec4 shape{lattice->lattice_width, lattice->lattice_height,
                  lattice->lattice_depth, lattice->terrain_elevation_cells};
  command.pushConstants<glm::vec4>(*pipeline_layout_,
                                   vk::ShaderStageFlagBits::eVertex, 0, shape);
  command.draw(36, lattice->cell_count, 0, 0);
}
}  // namespace renderer
