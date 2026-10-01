#include "rheo/runtime/application_backend.h"

#include "rheo/platform/window.h"

namespace runtime {

ApplicationBackend::ApplicationBackend(Config const& config)
    : window_(config.window_properties, input_queue_) {
  auto const extensions = platform::Window::RequiredGraphicsExtensions();
  context_.Init(extensions);
  context_.CreateSurface(window_);
  device_.Init(context_, *context_.Surface());
  swap_chain_.Init(device_, *context_.Surface(), window_);
  command_pools_.Init(device_);
  frame_sync_.Init(device_);
}

void ApplicationBackend::PollEvents() { platform::Window::PollEvents(); }

bool ApplicationBackend::IsMinimized() const {
  auto size = window_.Size();
  return size.width == 0 || size.height == 0;
}

void ApplicationBackend::WaitEvents() { platform::Window::WaitEvents(); }

void ApplicationBackend::WaitIdle() { device_.LogicalDevice().waitIdle(); }

bool ApplicationBackend::ShouldClose() const { return window_.ShouldClose(); }

bool ApplicationBackend::NeedsSwapChainRecreation() const {
  auto size = window_.Size();
  return size.width != int(swap_chain_.Extent().width) ||
         size.height != int(swap_chain_.Extent().height);
}

bool ApplicationBackend::RecreateSwapChain() {
  if (IsMinimized()) {
    return false;
  }
  swap_chain_.RecreateSwapChain(device_, window_);
  return true;
}

}  // namespace runtime
