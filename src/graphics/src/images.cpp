#include "rheo/graphics/images.h"

#include <array>
#include <format>
#include <stdexcept>
#include <utility>

#include "rheo/graphics/buffer.h"
#include "rheo/graphics/immediate_submit.h"
#include "rheo/graphics/memory.h"

graphics::AllocatedImage::~AllocatedImage() {
  image_view_.clear();
  sampler_.clear();
  if (allocator_ != nullptr) {
    vmaDestroyImage(allocator_, static_cast<VkImage>(image_.release()),
                    allocation_);
  }
}

graphics::AllocatedImage::AllocatedImage(AllocatedImage&& other) noexcept {
  Swap(other);
}

graphics::AllocatedImage& graphics::AllocatedImage::operator=(
    AllocatedImage&& other) noexcept {
  if (this != &other) {
    AllocatedImage previous(std::move(other));
    Swap(previous);
  }
  return *this;
}

void graphics::AllocatedImage::Swap(AllocatedImage& other) noexcept {
  std::swap(allocator_, other.allocator_);
  std::swap(allocation_, other.allocation_);
  image_.swap(other.image_);
  image_view_.swap(other.image_view_);
  sampler_.swap(other.sampler_);
}

graphics::AllocatedImage graphics::ImageAllocator::CreateImage(
    Device const& device, vk::ImageCreateInfo const& image_info) {
  VmaAllocationCreateInfo allocation_info{
      .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };
  AllocatedImage result;
  result.allocator_ = device.Allocator();
  VkImage raw_image = VK_NULL_HANDLE;
  auto status = vmaCreateImage(
      result.allocator_,
      reinterpret_cast<VkImageCreateInfo const*>(&image_info), &allocation_info,
      &raw_image, &result.allocation_, nullptr);
  if (status != VK_SUCCESS) {
    throw std::runtime_error(std::format(
        "[ERROR] Graphics: failed to allocate image ({})", int(status)));
  }
  result.image_ = vk::raii::Image(device.LogicalDevice(), raw_image);
  return result;
}

graphics::AllocatedImage graphics::ImageAllocator::UploadImage(
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
  staging.WriteMapped(pixels, size);

  const std::array queue_families{device.ComputeQueueFamilyIndex(),
                                  device.GraphicsQueueFamilyIndex()};
  bool const shared = queue_families[0] != queue_families[1];
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
      .sharingMode =
          shared ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
      .queueFamilyIndexCount =
          shared ? static_cast<std::uint32_t>(queue_families.size()) : 0U,
      .pQueueFamilyIndices = queue_families.data(),
      .initialLayout = vk::ImageLayout::eUndefined};

  auto result = graphics::ImageAllocator::CreateImage(device, image_info);
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
      .image = result.Image(),
      .viewType = vk::ImageViewType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  result.image_view_ = vk::raii::ImageView(device.LogicalDevice(), view_info);

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
  result.sampler_ = vk::raii::Sampler(device.LogicalDevice(), sampler_info);
  return result;
}

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

graphics::AllocatedImage graphics::ImageAllocator::CreateDepthImage(
    Device const& device, vk::Extent2D extent, vk::Format format) {
  auto result = CreateImage(
      device,
      {.imageType = vk::ImageType::e2D,
       .format = format,
       .extent = {.width = extent.width, .height = extent.height, .depth = 1},
       .mipLevels = 1,
       .arrayLayers = 1,
       .samples = vk::SampleCountFlagBits::e1,
       .tiling = vk::ImageTiling::eOptimal,
       .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
       .sharingMode = vk::SharingMode::eExclusive});
  result.image_view_ = vk::raii::ImageView(
      device.LogicalDevice(),
      {.image = *result.image_,
       .viewType = vk::ImageViewType::e2D,
       .format = format,
       .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1}});
  return result;
}
