#include "rheo/graphics/context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <format>
#include <stdexcept>
#include <vector>

void graphics::GraphicsContext::Init(
    std::vector<const char*> const& required_extensions) {
  CreateInstance(required_extensions);
}

void graphics::GraphicsContext::CreateSurface(platform::Window const& window) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult const result = glfwCreateWindowSurface(
      *instance_, static_cast<GLFWwindow*>(window.NativeHandle()), nullptr,
      &surface);
  if (result != VK_SUCCESS) {
    throw std::runtime_error(
        std::format("[ERROR] Graphics: failed to create window surface ({})",
                    static_cast<int>(result)));
  }
  surface_ = vk::raii::SurfaceKHR(instance_, surface);
}

void graphics::GraphicsContext::CreateInstance(
    std::vector<const char*> const& required_extensions) {
  constexpr vk::ApplicationInfo kAppInfo{
      .pApplicationName = "Rheo LBM",
      .applicationVersion = VK_MAKE_VERSION(0, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
      .apiVersion = vk::ApiVersion14};

  auto extension_properties = context_.enumerateInstanceExtensionProperties();
  for (const auto* extension : required_extensions) {
    if (std::ranges::none_of(
            extension_properties, [extension](auto const& extension_property) {
              return strcmp(extension_property.extensionName, extension) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required window extension not supported: " +
          std::string(extension));
    }
  }

  std::vector<const char*> layers{};
  if (kEnableValidationLayers) {
    layers.assign(kValidationLayers.begin(), kValidationLayers.end());
  }

  auto layer_properties = context_.enumerateInstanceLayerProperties();
  for (const auto* layer : layers) {
    if (std::ranges::none_of(
            layer_properties, [layer](auto const& layer_property) {
              return strcmp(layer_property.layerName, layer) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required layer not supported: " +
          std::string(layer));
    }
  }

  const vk::InstanceCreateInfo create_info{
      .pApplicationInfo = &kAppInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(required_extensions.size()),
      .ppEnabledExtensionNames = required_extensions.data(),
  };

  instance_ = vk::raii::Instance(context_, create_info);
}
