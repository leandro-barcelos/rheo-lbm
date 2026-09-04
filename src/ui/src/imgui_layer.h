#ifndef RHEOLBM_IMGUI_LAYER_H
#define RHEOLBM_IMGUI_LAYER_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "rheo/graphics/context.h"
#include "rheo/graphics/device.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"

namespace ui {

class ImGuiLayer {
 public:
  ImGuiLayer() = default;
  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer(ImGuiLayer&&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(ImGuiLayer&&) = delete;
  ~ImGuiLayer();

  void Init(platform::Window const& window,
            graphics::GraphicsContext const& context,
            graphics::Device const& device,
            graphics::SwapChain const& swap_chain);
  void BeginFrame() const;
  void EndFrame() const;
  void Render(vk::CommandBuffer command_buffer) const;

  void OnFrameResourcesChanged(std::uint32_t image_count);
  void Shutdown();

 private:
  static void CheckVkResult(VkResult result);
  static void SetupImGuiStyle();

  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  uint32_t min_image_count_ = 0;
  bool initialized_ = false;
};

}  // namespace ui

#endif  // !RHEOLBM_IMGUI_LAYER_H
