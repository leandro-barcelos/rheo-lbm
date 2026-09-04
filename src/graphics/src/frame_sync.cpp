#include "rheo/graphics/frame_sync.h"

void graphics::FrameSync::Init(graphics::Device const& device,
                               uint32_t max_frames_in_flight) {
  max_frames_in_flight_ = max_frames_in_flight;
  in_flight_fences_.clear();

  vk::SemaphoreTypeCreateInfo semaphore_type{
      .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0};
  vk::SemaphoreCreateInfo semaphore_info{.pNext = &semaphore_type};
  semaphore_ = vk::raii::Semaphore(device.LogicalDevice(), semaphore_info);
  timeline_value_ = 0;

  for (size_t i = 0; i < max_frames_in_flight_; i++) {
    vk::FenceCreateInfo fence_info{};
    in_flight_fences_.emplace_back(device.LogicalDevice(), fence_info);
  }
}

void graphics::FrameSync::WaitForFence(graphics::Device const& device) const {
  auto fence_result =
      device.LogicalDevice().waitForFences(*Fence(), vk::True, UINT64_MAX);
  if (fence_result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for fence!");
  }
  device.LogicalDevice().resetFences(*Fence());
}

void graphics::FrameSync::WaitSemaphore(graphics::Device const& device,
                                        uint64_t wait_value) const {
  vk::SemaphoreWaitInfo wait_info{
      .semaphoreCount = 1, .pSemaphores = &*semaphore_, .pValues = &wait_value};

  auto result = device.LogicalDevice().waitSemaphores(wait_info, UINT64_MAX);
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for semaphore!");
  }
}
