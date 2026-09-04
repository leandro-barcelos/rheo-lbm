#ifndef RHEO_GRAPHICS_CONTEXT_H
#define RHEO_GRAPHICS_CONTEXT_H

#include <array>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/platform/window.h"

namespace graphics {

constexpr std::array kValidationLayers{"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

class GraphicsContext {
 public:
  void Init(std::vector<const char*> const& required_extensions);
  void CreateSurface(platform::Window const& window);

  [[nodiscard]] vk::raii::Context const& Context() const { return context_; }
  [[nodiscard]] vk::raii::Instance const& Instance() const { return instance_; }
  [[nodiscard]] vk::raii::SurfaceKHR const& Surface() const { return surface_; }

 private:
  vk::raii::Context context_;
  vk::raii::Instance instance_ = nullptr;
  vk::raii::SurfaceKHR surface_ = nullptr;

  void CreateInstance(std::vector<const char*> const& required_extensions);
};
}  // namespace graphics

#endif  // !RHEO_GRAPHICS_CONTEXT_H
