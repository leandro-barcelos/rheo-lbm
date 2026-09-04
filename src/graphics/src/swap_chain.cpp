#include "rheo/graphics/swap_chain.h"

#include <array>
#include <stdexcept>
#include <vector>

#include "rheo/graphics/device.h"
#include "rheo/platform/window.h"
#include "vulkan/vulkan.hpp"

void graphics::SwapChain::Init(Device const& device, vk::SurfaceKHR surface,
                               platform::Window const& window) {
  surface_ = surface;
  CreateSwapChain(device, window);
  CreateImageViews(device);
}

uint32_t graphics::SwapChain::AcquireNextImage(
    graphics::Device const& device,
    graphics::FrameSync const& frame_sync) const {
  auto [result, image_index] =
      swap_chain_.acquireNextImage(UINT64_MAX, nullptr, *frame_sync.Fence());

  if (result == vk::Result::eErrorOutOfDateKHR) {
    return kInvalidImageIndex;
  }
  if ((result != vk::Result::eSuccess) &&
      (result != vk::Result::eSuboptimalKHR)) {
    throw std::runtime_error(
        "[ERROR] Vulkan: failed to acquire swap chain image!");
  }

  frame_sync.WaitForFence(device);
  return image_index;
}

void graphics::SwapChain::CreateSwapChain(Device const& device,
                                          platform::Window const& window) {
  vk::SurfaceCapabilitiesKHR const surface_capabilities =
      device.PhysicalDevice().getSurfaceCapabilitiesKHR(surface_);
  platform::WindowSize const window_size = window.Size();
  extent_ = ChooseExtent(window_size, surface_capabilities);

  uint32_t const min_image_count = ChooseMinImageCount(surface_capabilities);

  std::vector<vk::SurfaceFormatKHR> const available_formats =
      device.PhysicalDevice().getSurfaceFormatsKHR(surface_);
  surface_format_ = ChooseSurfaceFormat(available_formats);

  std::vector<vk::PresentModeKHR> const available_present_modes =
      device.PhysicalDevice().getSurfacePresentModesKHR(surface_);
  vk::PresentModeKHR const present_mode =
      ChoosePresentMode(available_present_modes);

  std::array<uint32_t, 2> queue_family_indices = {
      device.GraphicsQueueFamilyIndex(), device.PresentQueueFamilyIndex()};
  const bool separate_present_queue =
      queue_family_indices[0] != queue_family_indices[1];

  vk::SwapchainCreateInfoKHR const swap_chain_create_info{
      .surface = surface_,
      .minImageCount = min_image_count,
      .imageFormat = surface_format_.format,
      .imageColorSpace = surface_format_.colorSpace,
      .imageExtent = extent_,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = separate_present_queue ? vk::SharingMode::eConcurrent
                                                 : vk::SharingMode::eExclusive,
      .queueFamilyIndexCount = separate_present_queue ? 2U : 0U,
      .pQueueFamilyIndices =
          separate_present_queue ? queue_family_indices.data() : nullptr,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = present_mode,
      .clipped = vk::True};

  swap_chain_ =
      vk::raii::SwapchainKHR(device.LogicalDevice(), swap_chain_create_info);
  images_ = swap_chain_.getImages();
  image_layouts_.assign(images_.size(), vk::ImageLayout::eUndefined);
}

void graphics::SwapChain::CreateImageViews(Device const& device) {
  assert(image_views_.empty());

  vk::ImageViewCreateInfo image_view_create_info{
      .viewType = vk::ImageViewType::e2D,
      .format = surface_format_.format,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  for (const auto& image : images_) {
    image_view_create_info.image = image;
    image_views_.emplace_back(device.LogicalDevice(), image_view_create_info);
  }
}

vk::Extent2D graphics::SwapChain::ChooseExtent(
    platform::WindowSize const window_size,
    vk::SurfaceCapabilitiesKHR const& surface_capabilities) {
  if (surface_capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return surface_capabilities.currentExtent;
  }
  return {.width = std::clamp<uint32_t>(
              window_size.width, surface_capabilities.minImageExtent.width,
              surface_capabilities.maxImageExtent.width),
          .height = std::clamp<uint32_t>(
              window_size.height, surface_capabilities.minImageExtent.height,
              surface_capabilities.maxImageExtent.height)};
}

vk::SurfaceFormatKHR graphics::SwapChain::ChooseSurfaceFormat(
    std::vector<vk::SurfaceFormatKHR> const& available_formats) {
  assert(!available_formats.empty());
  const auto unorm_format_it =
      std::ranges::find_if(available_formats, [](const auto& format) {
        return format.format == vk::Format::eB8G8R8A8Unorm &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
  if (unorm_format_it != available_formats.end()) {
    return *unorm_format_it;
  }

  const auto srgb_format_it =
      std::ranges::find_if(available_formats, [](const auto& format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });

  return srgb_format_it != available_formats.end() ? *srgb_format_it
                                                   : available_formats.at(0);
}

uint32_t graphics::SwapChain::ChooseMinImageCount(
    vk::SurfaceCapabilitiesKHR const& surface_capabilities) {
  auto min_image_count = std::max(3U, surface_capabilities.minImageCount);
  if ((0 < surface_capabilities.maxImageCount) &&
      (surface_capabilities.maxImageCount < min_image_count)) {
    min_image_count = surface_capabilities.maxImageCount;
  }
  return min_image_count;
}

vk::PresentModeKHR graphics::SwapChain::ChoosePresentMode(
    std::vector<vk::PresentModeKHR> const& available_present_modes) {
  assert(std::ranges::any_of(available_present_modes, [](auto present_mode) {
    return present_mode == vk::PresentModeKHR::eFifo;
  }));
  return std::ranges::any_of(available_present_modes,
                             [](const vk::PresentModeKHR value) {
                               return vk::PresentModeKHR::eMailbox == value;
                             })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

void graphics::SwapChain::CleanupSwapChain() {
  image_views_.clear();
  image_layouts_.clear();
  swap_chain_ = nullptr;
}

void graphics::SwapChain::RecreateSwapChain(Device const& device,
                                            platform::Window const& window) {
  platform::WindowSize window_size = window.Size();
  while (window_size.width == 0 || window_size.height == 0) {
    window_size = window.Size();
    platform::Window::WaitEvents();
  }

  device.LogicalDevice().waitIdle();

  CleanupSwapChain();
  CreateSwapChain(device, window);
  CreateImageViews(device);
}
