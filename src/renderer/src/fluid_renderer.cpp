#include "fluid_renderer.h"

#include <cstddef>
#include <optional>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "camera.h"
#include "rheo/graphics/buffer.h"
#include "rheo/graphics/pipeline.h"
#include "vulkan/vulkan.hpp"

namespace {

vk::VertexInputBindingDescription FluidBindingDescription() {
  return {.binding = 0,
          .stride = sizeof(simulation::FluidParticle),
          .inputRate = vk::VertexInputRate::eVertex};
}

std::vector<vk::VertexInputAttributeDescription> FluidAttributeDescriptions() {
  using Particle = simulation::FluidParticle;
  return {
      {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Particle, position)},
      {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Particle, velocity)},
      {2, 0, vk::Format::eR32Sfloat, offsetof(Particle, density)},
  };
}

}  // namespace

void renderer::FluidRenderer::Init(graphics::Device const& device,
                                   graphics::SwapChain const& swap_chain) {
  CreateCameraDescriptorSetLayout(device);
  const std::array camera_descriptor_pool_sizes = {
      graphics::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1}};
  camera_descriptor_allocator_.Init(device, 1, camera_descriptor_pool_sizes);
  CreateGraphicsPipeline(device, swap_chain);
  CreateBuffers(device);
  CreateCameraDescriptorSet(device, camera_descriptor_allocator_);
}

void renderer::FluidRenderer::Render(
    vk::raii::CommandBuffer const& command_buffer,
    graphics::SwapChain& swap_chain,
    std::optional<simulation::FluidRenderSnapshot> const& fluid,
    renderer::Camera const& camera) {
  UpdateUniformBuffer(swap_chain, camera);

  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              *graphics_pipeline_);
  command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                    *graphics_pipeline_layout_, 0,
                                    {*camera_descriptor_set_}, {});
  command_buffer.setViewport(
      0,
      vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain.Extent().width),
                   static_cast<float>(swap_chain.Extent().height), 0.0F, 1.0F));
  command_buffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain.Extent()));
  if (fluid) {
    auto const buffer = vk::Buffer(
        reinterpret_cast<VkBuffer>(fluid->particle_buffer.native_handle));
    command_buffer.bindVertexBuffers(0, {buffer}, {0});
    command_buffer.draw(fluid->particle_count, 1, 0, 0);
  }
}

void renderer::FluidRenderer::CreateGraphicsPipeline(
    graphics::Device const& device, graphics::SwapChain const& swap_chain) {
  auto binding_description = FluidBindingDescription();
  auto attribute_descriptions = FluidAttributeDescriptions();

  const std::array<vk::DescriptorSetLayout, 1> set_layouts = {
      *camera_descriptor_set_layout_};
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
      .pSetLayouts = set_layouts.data()};
  graphics_pipeline_layout_ =
      vk::raii::PipelineLayout(device.LogicalDevice(), pipeline_layout_info);

  graphics_pipeline_ = graphics::PipelineBuilder::Graphics(
      device, binding_description, attribute_descriptions,
      graphics_pipeline_layout_, swap_chain, "shaders/graphics/particle.spv");
}

void renderer::FluidRenderer::CreateCameraDescriptorSetLayout(
    graphics::Device const& device) {
  vk::DescriptorSetLayoutBinding ubo_layout_binding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .pImmutableSamplers = nullptr};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = 1, .pBindings = &ubo_layout_binding};

  camera_descriptor_set_layout_ =
      device.LogicalDevice().createDescriptorSetLayout(layout_info);
}

void renderer::FluidRenderer::CreateBuffers(graphics::Device const& device) {
  camera_ubo_buffer_ = graphics::BufferAllocator::CreateMappedUniformBuffer(
      device, sizeof(CameraUBO));
}

void renderer::FluidRenderer::CreateCameraDescriptorSet(
    graphics::Device const& device,
    graphics::DescriptorAllocator const& descriptor_allocator) {
  camera_descriptor_set_ =
      descriptor_allocator.Allocate(device, camera_descriptor_set_layout_);

  vk::DescriptorBufferInfo camera_buffer_info(camera_ubo_buffer_.buffer, 0,
                                              sizeof(CameraUBO));

  vk::WriteDescriptorSet descriptor_write{
      .dstSet = *camera_descriptor_set_,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .pBufferInfo = &camera_buffer_info};

  device.LogicalDevice().updateDescriptorSets(
      vk::ArrayProxy<const vk::WriteDescriptorSet>(descriptor_write), {});
}

void renderer::FluidRenderer::UpdateUniformBuffer(
    graphics::SwapChain const& swap_chain, renderer::Camera const& camera) {
  float aspect_ratio = static_cast<float>(swap_chain.Extent().width) /
                       static_cast<float>(swap_chain.Extent().height);
  CameraUBO camera_ubo{.model = {1.0F},
                       .view = camera.ViewMatrix(),
                       .proj = camera.ProjectionMatrix(aspect_ratio)};
  camera_ubo.proj[1][1] *= -1;

  graphics::BufferAllocator::WriteMapped(camera_ubo_buffer_, &camera_ubo,
                                         sizeof(CameraUBO));
}
