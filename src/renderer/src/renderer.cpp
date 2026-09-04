#include "rheo/renderer/renderer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "camera.h"
#include "fluid_renderer.h"
#include "terrain_renderer.h"

namespace renderer {

class Renderer::Impl {
 public:
  explicit Impl(platform::WindowSize initial_window_size)
      : camera_(glm::vec3(0.5F, 2.0F, 0.5F), initial_window_size) {}

  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain,
            graphics::CommandPools const& command_pools) {
    command_pools_ = &command_pools;
    fluid_renderer_.Init(device, swap_chain);
    vk::CommandBufferAllocateInfo allocation{
        .commandPool = *command_pools.Graphics(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};
    command_buffer_ = std::move(
        device.LogicalDevice().allocateCommandBuffers(allocation).front());
  }

  void HandleInput(events::InputEvent const& event) {
    camera_.HandleInput(event);
  }
  void RequestResize() { resize_pending_ = true; }

  void RenderFrame(graphics::Device const& device,
                   graphics::SwapChain& swap_chain,
                   graphics::FrameSync& frame_sync,
                   application::SceneState const& scene,
                   platform::Window const& window, IOverlayPass& overlay) {
    if (resize_pending_) {
      RecreateSwapChain(device, swap_chain, window, overlay);
    }

    std::uint32_t const image_index =
        swap_chain.AcquireNextImage(device, frame_sync);
    if (image_index == graphics::SwapChain::kInvalidImageIndex) {
      RecreateSwapChain(device, swap_chain, window, overlay);
      return;
    }

    SyncScene(device, swap_chain, scene);
    auto const simulation_signal =
        scene.fluid && scene.fluid->ready_signal > 0
            ? std::optional<std::uint64_t>(scene.fluid->ready_signal)
            : std::nullopt;
    std::uint64_t const wait_value = simulation_signal.value_or(0);
    std::uint64_t const signal_value = frame_sync.GetNextTimelineValue();

    command_buffer_.reset();
    command_buffer_.begin({});
    TransitionImage(swap_chain, image_index,
                    swap_chain.GetImageLayout(image_index),
                    vk::ImageLayout::eColorAttachmentOptimal, {},
                    vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    swap_chain.SetImageLayout(image_index,
                              vk::ImageLayout::eColorAttachmentOptimal);

    vk::ClearValue clear_color = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
    vk::RenderingAttachmentInfo attachment{
        .imageView = swap_chain.GetImageView(image_index),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_color};
    vk::RenderingInfo rendering{.renderArea = {.offset = {.x = 0, .y = 0},
                                               .extent = swap_chain.Extent()},
                                .layerCount = 1,
                                .colorAttachmentCount = 1,
                                .pColorAttachments = &attachment};
    command_buffer_.beginRendering(rendering);
    terrain_renderer_.Render(command_buffer_, swap_chain, camera_);
    fluid_renderer_.Render(command_buffer_, swap_chain, scene.fluid, camera_);
    overlay.Render(graphics::CommandList{
        .native_handle = static_cast<VkCommandBuffer>(*command_buffer_)});
    command_buffer_.endRendering();

    TransitionImage(swap_chain, image_index,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits2::eBottomOfPipe);
    swap_chain.SetImageLayout(image_index, vk::ImageLayout::ePresentSrcKHR);
    command_buffer_.end();

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eVertexInput;
    vk::TimelineSemaphoreSubmitInfo timeline{
        .waitSemaphoreValueCount = simulation_signal ? 1U : 0U,
        .pWaitSemaphoreValues = simulation_signal ? &wait_value : nullptr,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &signal_value};
    vk::SubmitInfo submit{
        .pNext = &timeline,
        .waitSemaphoreCount = simulation_signal ? 1U : 0U,
        .pWaitSemaphores =
            simulation_signal ? &*frame_sync.Semaphore() : nullptr,
        .pWaitDstStageMask = simulation_signal ? &wait_stage : nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer_,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*frame_sync.Semaphore()};
    device.GraphicsQueue().submit(submit, nullptr);
    frame_sync.WaitSemaphore(device, signal_value);

    vk::PresentInfoKHR present{.swapchainCount = 1,
                               .pSwapchains = &*swap_chain.Handle(),
                               .pImageIndices = &image_index};
    auto const result = device.PresentQueue().presentKHR(present);
    if (result == vk::Result::eSuboptimalKHR ||
        result == vk::Result::eErrorOutOfDateKHR) {
      RecreateSwapChain(device, swap_chain, window, overlay);
    } else {
      assert(result == vk::Result::eSuccess);
    }
  }

  void Shutdown() {
    terrain_renderer_ = {};
    fluid_renderer_ = {};
    command_buffer_ = nullptr;
  }

 private:
  void SyncScene(graphics::Device const& device,
                 graphics::SwapChain const& swap_chain,
                 application::SceneState const& scene) {
    if (scene.revision == loaded_scene_revision_) {
      return;
    }
    loaded_scene_revision_ = scene.revision;
    if (scene.terrain == nullptr || command_pools_ == nullptr) {
      terrain_renderer_.Clear(device);
      return;
    }

    terrain_renderer_.Init(device, swap_chain, *command_pools_, scene.terrain,
                           scene.terrain_texture);
    glm::vec3 bounds_min(std::numeric_limits<float>::infinity());
    glm::vec3 bounds_max(-std::numeric_limits<float>::infinity());
    for (auto const& sample : scene.terrain->samples) {
      bounds_min = glm::min(bounds_min, sample.position);
      bounds_max = glm::max(bounds_max, sample.position);
    }
    camera_.InitTopView(bounds_min, bounds_max);
  }

  void RecreateSwapChain(graphics::Device const& device,
                         graphics::SwapChain& swap_chain,
                         platform::Window const& window,
                         IOverlayPass& overlay) {
    swap_chain.RecreateSwapChain(device, window);
    overlay.OnFrameResourcesChanged(swap_chain.ImageCount());
    resize_pending_ = false;
  }

  void TransitionImage(graphics::SwapChain const& swap_chain,
                       std::uint32_t image_index, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout,
                       vk::AccessFlags2 source_access,
                       vk::AccessFlags2 destination_access,
                       vk::PipelineStageFlags2 source_stage,
                       vk::PipelineStageFlags2 destination_stage) {
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = source_stage,
        .srcAccessMask = source_access,
        .dstStageMask = destination_stage,
        .dstAccessMask = destination_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = swap_chain.GetImage(image_index),
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vk::DependencyInfo dependency{.imageMemoryBarrierCount = 1,
                                  .pImageMemoryBarriers = &barrier};
    command_buffer_.pipelineBarrier2(dependency);
  }

  vk::raii::CommandBuffer command_buffer_ = nullptr;
  FluidRenderer fluid_renderer_;
  TerrainRenderer terrain_renderer_;
  Camera camera_;
  graphics::CommandPools const* command_pools_ = nullptr;
  std::uint64_t loaded_scene_revision_ =
      std::numeric_limits<std::uint64_t>::max();
  bool resize_pending_ = false;
};

Renderer::Renderer(platform::WindowSize initial_window_size)
    : impl_(std::make_unique<Impl>(initial_window_size)) {}
Renderer::~Renderer() = default;

void Renderer::Init(graphics::Device const& device,
                    graphics::SwapChain const& swap_chain,
                    graphics::CommandPools const& command_pools) {
  impl_->Init(device, swap_chain, command_pools);
}

void Renderer::RenderFrame(graphics::Device const& device,
                           graphics::SwapChain& swap_chain,
                           graphics::FrameSync& frame_sync,
                           application::SceneState const& scene,
                           platform::Window const& window,
                           IOverlayPass& overlay) {
  impl_->RenderFrame(device, swap_chain, frame_sync, scene, window, overlay);
}

void Renderer::HandleInput(events::InputEvent const& event) {
  impl_->HandleInput(event);
}
void Renderer::RequestResize() { impl_->RequestResize(); }
void Renderer::Shutdown() { impl_->Shutdown(); }

}  // namespace renderer
