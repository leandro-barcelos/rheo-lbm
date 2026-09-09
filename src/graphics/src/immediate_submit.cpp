#include "rheo/graphics/immediate_submit.h"

#include <stdexcept>

#include "vulkan/vulkan.hpp"

void graphics::ImmediateSubmit::CopyBuffer(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    graphics::AllocatedBuffer const& src_buffer,
    graphics::AllocatedBuffer const& dst_buffer, vk::DeviceSize size) {
  BeginSingleTimeCommands(device, command_pools);
  command_buffer_.copyBuffer(src_buffer.Buffer(), dst_buffer.Buffer(),
                             vk::BufferCopy(0, 0, size));
  EndSingleTimeCommands(device);
}

void graphics::ImmediateSubmit::CopyBufferToImage(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    graphics::AllocatedBuffer const& buffer,
    graphics::AllocatedImage const& image, uint32_t width, uint32_t height) {
  BeginSingleTimeCommands(device, command_pools);
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {.x = 0, .y = 0, .z = 0},
      .imageExtent = {.width = width, .height = height, .depth = 1}};
  command_buffer_.copyBufferToImage(buffer.Buffer(), image.Image(),
                                    vk::ImageLayout::eTransferDstOptimal,
                                    {region});
  EndSingleTimeCommands(device);
}

void graphics::ImmediateSubmit::TransitionImageLayout(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    graphics::AllocatedImage const& image, vk::ImageLayout old_layout,
    vk::ImageLayout new_layout) {
  BeginSingleTimeCommands(device, command_pools);

  vk::ImageMemoryBarrier barrier{
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .image = image.Image(),
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  vk::PipelineStageFlags source_stage;
  vk::PipelineStageFlags destination_stage;

  if (old_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eTransfer;
  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    source_stage = vk::PipelineStageFlagBits::eTransfer;
    destination_stage = vk::PipelineStageFlagBits::eComputeShader;
  } else {
    throw std::invalid_argument(
        "[ERROR] Vulkan: unsupported layout transition!");
  }

  command_buffer_.pipelineBarrier(source_stage, destination_stage, {}, {},
                                  nullptr, barrier);
  EndSingleTimeCommands(device);
}

void graphics::ImmediateSubmit::BeginSingleTimeCommands(
    graphics::Device const& device,
    graphics::CommandPools const& command_pools) {
  if (command_buffer_ != nullptr) {
    throw std::runtime_error(
        "[ERROR] Buffer: tried to start a single time command that hasn't "
        "been "
        "finished yet!");
  }

  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = *command_pools.Graphics();
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = 1;
  command_buffer_ = std::move(
      vk::raii::CommandBuffers(device.LogicalDevice(), alloc_info).front());

  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  command_buffer_.begin(begin_info);
}

void graphics::ImmediateSubmit::EndSingleTimeCommands(
    graphics::Device const& device) {
  command_buffer_.end();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &*command_buffer_;
  device.GraphicsQueue().submit(submit_info, nullptr);
  device.GraphicsQueue().waitIdle();

  command_buffer_ = nullptr;
}
