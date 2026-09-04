#include "rheo/platform/window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <stdexcept>

#include "rheo/events/input_event.h"
#include "rheo/events/input_queue.h"

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace {

[[nodiscard]] bool IsRenderDocInjected() {
#if defined(__linux__) && defined(RTLD_NOLOAD)
  constexpr std::array<const char*, 2> kRenderDocLibNames = {
      "librenderdoc.so", "librenderdoc.so.1"};
  return std::ranges::any_of(kRenderDocLibNames, [](const char* lib_name) {
    if (void* handle = dlopen(lib_name, RTLD_NOW | RTLD_NOLOAD);
        handle != nullptr) {
      dlclose(handle);
      return true;
    }
    return false;
  });
#else
  return false;
#endif
}

[[nodiscard]] platform::Window* Owner(GLFWwindow* window) {
  return static_cast<platform::Window*>(glfwGetWindowUserPointer(window));
}

}  // namespace

platform::Window::Window(WindowProperties properties,
                         events::InputQueue& input_queue)
    : input_queue_(&input_queue) {
  glfwSetErrorCallback(ErrorCallback);

#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
  if (IsRenderDocInjected()) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  }
#endif

  if (glfwInit() == 0) {
    const char* description = nullptr;
    int const error_code = glfwGetError(&description);
    throw std::runtime_error(std::format(
        "[ERROR] Platform: failed to initialize windowing ({}): {}", error_code,
        description != nullptr ? description : "<no details>"));
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  auto* native_window = glfwCreateWindow(properties.width, properties.height,
                                         properties.title, nullptr, nullptr);
  if (native_window == nullptr) {
    throw std::runtime_error("[ERROR] Platform: failed to create window");
  }
  window_ = native_window;
  glfwSetWindowUserPointer(native_window, this);

  glfwSetWindowSizeCallback(native_window,
                            [](GLFWwindow* handle, int width, int height) {
                              Owner(handle)->InputEvents().Push(
                                  events::WindowResizedEvent{width, height});
                            });
  glfwSetKeyCallback(native_window, [](GLFWwindow* handle, int key, int,
                                       int action, int) {
    auto& queue = Owner(handle)->InputEvents();
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
      queue.Push(events::KeyPressedEvent{static_cast<events::KeyCode>(key),
                                         action == GLFW_REPEAT});
    } else if (action == GLFW_RELEASE) {
      queue.Push(events::KeyReleasedEvent{static_cast<events::KeyCode>(key)});
    }
  });
  glfwSetCursorPosCallback(
      native_window, [](GLFWwindow* handle, double x, double y) {
        Owner(handle)->InputEvents().Push(events::MouseMovedEvent{
            static_cast<float>(x), static_cast<float>(y)});
      });
  glfwSetMouseButtonCallback(
      native_window, [](GLFWwindow* handle, int button, int action, int) {
        auto& queue = Owner(handle)->InputEvents();
        if (action == GLFW_PRESS) {
          queue.Push(events::MouseButtonPressedEvent{
              static_cast<events::MouseCode>(button), false});
        } else if (action == GLFW_RELEASE) {
          queue.Push(events::MouseButtonReleasedEvent{
              static_cast<events::MouseCode>(button)});
        }
      });
  glfwSetScrollCallback(
      native_window, [](GLFWwindow* handle, double, double y_offset) {
        Owner(handle)->InputEvents().Push(
            events::MouseScrolledEvent{static_cast<float>(y_offset)});
      });
}

platform::Window::~Window() {
  glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
  glfwTerminate();
}

bool platform::Window::ShouldClose() const {
  return glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)) != 0;
}

void platform::Window::RequestClose() {
  glfwSetWindowShouldClose(static_cast<GLFWwindow*>(window_), GLFW_TRUE);
}

platform::WindowSize platform::Window::Size() const {
  WindowSize size;
  glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &size.width,
                         &size.height);
  return size;
}

void platform::Window::PollEvents() { glfwPollEvents(); }

void platform::Window::WaitEvents() { glfwWaitEvents(); }

double platform::Window::TimeSeconds() { return glfwGetTime(); }

std::vector<const char*> platform::Window::RequiredGraphicsExtensions() {
  std::uint32_t extension_count = 0;
  auto const* extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  return {extensions, extensions + extension_count};
}

void platform::Window::ErrorCallback(int error, const char* description) {
  std::cerr << std::format(
                   "[ERROR] Platform: [{}] {}", error,
                   description != nullptr ? description : "<no description>")
            << '\n';
}
