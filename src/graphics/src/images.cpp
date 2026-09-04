#include "rheo/graphics/images.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "rheo/graphics/buffer.h"
#include "rheo/graphics/immediate_submit.h"
#include "rheo/graphics/memory.h"

namespace {

graphics::AllocatedImage UploadImage(
    graphics::Device const& device, graphics::CommandPools const& command_pools,
    const void* pixels, std::uint32_t width, std::uint32_t height,
    vk::DeviceSize size) {
  if (pixels == nullptr || width == 0 || height == 0 || size == 0) {
    throw std::runtime_error("[ERROR] Graphics: invalid image data");
  }

  auto staging = graphics::BufferAllocator::CreateBuffer(
      device, size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);
  void* mapped = staging.memory.mapMemory(0, size);
  std::memcpy(mapped, pixels, static_cast<std::size_t>(size));
  staging.memory.unmapMemory();

  const std::array queue_families{device.ComputeQueueFamilyIndex(),
                                  device.GraphicsQueueFamilyIndex()};
  vk::ImageCreateInfo image_info{
      .imageType = vk::ImageType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .extent = {.width = width, .height = height, .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eTransferDst |
               vk::ImageUsageFlagBits::eSampled,
      .sharingMode = vk::SharingMode::eConcurrent,
      .queueFamilyIndexCount =
          static_cast<std::uint32_t>(queue_families.size()),
      .pQueueFamilyIndices = queue_families.data(),
      .initialLayout = vk::ImageLayout::eUndefined};

  vk::raii::Image vk_image{device.LogicalDevice(), image_info};
  auto requirements = vk_image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocation{
      .allocationSize = requirements.size,
      .memoryTypeIndex = graphics::MemoryAllocator::FindMemoryType(
          device, requirements.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eDeviceLocal)};
  vk::raii::DeviceMemory memory{device.LogicalDevice(), allocation};
  vk_image.bindMemory(memory, 0);

  graphics::AllocatedImage result{.memory = std::move(memory),
                                  .image = std::move(vk_image)};
  graphics::ImmediateSubmit submit;
  submit.TransitionImageLayout(device, command_pools, result,
                               vk::ImageLayout::eUndefined,
                               vk::ImageLayout::eTransferDstOptimal);
  submit.CopyBufferToImage(device, command_pools, staging, result, width,
                           height);
  submit.TransitionImageLayout(device, command_pools, result,
                               vk::ImageLayout::eTransferDstOptimal,
                               vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::ImageViewCreateInfo view_info{
      .image = result.image,
      .viewType = vk::ImageViewType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  result.image_view = vk::raii::ImageView(device.LogicalDevice(), view_info);

  vk::SamplerCreateInfo sampler_info{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eClampToEdge,
      .addressModeV = vk::SamplerAddressMode::eClampToEdge,
      .addressModeW = vk::SamplerAddressMode::eClampToEdge,
      .anisotropyEnable = vk::False,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways};
  result.sampler = vk::raii::Sampler(device.LogicalDevice(), sampler_info);
  return result;
}

}  // namespace

graphics::AllocatedImage graphics::ImageAllocator::CreateImage(
    Device const& device, CommandPools const& command_pools,
    domain::ImageData const& image) {
  if (!image.IsValid()) {
    throw std::runtime_error("[ERROR] Graphics: invalid image data");
  }
  return UploadImage(device, command_pools, image.rgba.data(), image.width,
                     image.height, image.rgba.size());
}

graphics::AllocatedImage graphics::ImageAllocator::CreateSolidColorImage(
    Device const& device, CommandPools const& command_pools, std::uint8_t red,
    std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
  std::array<std::uint8_t, 4> const pixel{red, green, blue, alpha};
  return UploadImage(device, command_pools, pixel.data(), 1, 1, pixel.size());
}
