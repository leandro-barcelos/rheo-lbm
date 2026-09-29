#include "rheo/runtime/application_backend.h"

namespace runtime {

ApplicationBackend::ApplicationBackend(Config const& config)
    : window_(config.window_properties, input_queue_) {}

void ApplicationBackend::Init() {
  auto const extensions = platform::Window::RequiredGraphicsExtensions();
  context_.Init(extensions);
  context_.CreateSurface(window_);
  device_.Init(context_, *context_.Surface());
  swap_chain_.Init(device_, *context_.Surface(), window_);
  command_pools_.Init(device_);
  frame_sync_.Init(device_);
}

}  // namespace runtime
