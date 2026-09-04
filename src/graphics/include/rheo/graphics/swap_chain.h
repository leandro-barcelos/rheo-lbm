#ifndef RHEO_GRAPHICS_SWAP_CHAIN_H
#define RHEO_GRAPHICS_SWAP_CHAIN_H

#include <limits>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/frame_sync.h"
#include "rheo/platform/window.h"
#include "vulkan/vulkan.hpp"

namespace graphics {
class SwapChain {
 public:
  static constexpr uint32_t kInvalidImageIndex =
      std::numeric_limits<uint32_t>::max();

  void Init(Device const& device, vk::SurfaceKHR surface,
            platform::Window const& window);
  [[nodiscard]] uint32_t AcquireNextImage(
      graphics::Device const& device,
      graphics::FrameSync const& frame_sync) const;
  void RecreateSwapChain(Device const& device, platform::Window const& window);
  [[nodiscard]] vk::raii::SwapchainKHR const& Handle() const {
    return swap_chain_;
  }
  [[nodiscard]] vk::SurfaceFormatKHR const& SurfaceFormat() const {
    return surface_format_;
  }
  [[nodiscard]] vk::Image const& GetImage(uint32_t index) const {
    return images_.at(index);
  }
  [[nodiscard]] vk::ImageLayout const& GetImageLayout(uint32_t index) const {
    return image_layouts_.at(index);
  }
  void SetImageLayout(uint32_t index, vk::ImageLayout image_layout) {
    image_layouts_.at(index) = image_layout;
  }
  [[nodiscard]] vk::raii::ImageView const& GetImageView(uint32_t index) const {
    return image_views_.at(index);
  }
  [[nodiscard]] uint32_t ImageCount() const {
    return static_cast<uint32_t>(images_.size());
  }
  [[nodiscard]] vk::Extent2D const& Extent() const { return extent_; }

 private:
  vk::raii::SwapchainKHR swap_chain_ = nullptr;
  std::vector<vk::Image> images_;
  std::vector<vk::raii::ImageView> image_views_;
  std::vector<vk::ImageLayout> image_layouts_;
  vk::Extent2D extent_;
  vk::SurfaceFormatKHR surface_format_;
  vk::SurfaceKHR surface_ = nullptr;

  void CreateSwapChain(Device const& device, platform::Window const& window);
  void CreateImageViews(Device const& device);
  static vk::SurfaceFormatKHR ChooseSurfaceFormat(
      std::vector<vk::SurfaceFormatKHR> const& available_formats);
  static vk::Extent2D ChooseExtent(
      platform::WindowSize window_size,
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static uint32_t ChooseMinImageCount(
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static vk::PresentModeKHR ChoosePresentMode(
      std::vector<vk::PresentModeKHR> const& available_present_modes);
  void CleanupSwapChain();
};
}  // namespace graphics

#endif  // !RHEO_GRAPHICS_SWAP_CHAIN_H
