#include "rheo/renderer/renderer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "camera.h"
#include "lattice_renderer.h"
#include "rheo/graphics/memory.h"

namespace renderer {

class Renderer::Impl {
 public:
  explicit Impl(platform::WindowSize initial_window_size)
      : camera_(glm::vec3(0.5F, 2.0F, 0.5F), initial_window_size) {}

  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain,
            graphics::CommandPools const& command_pools) {
    CreateDepth(device, swap_chain.Extent());
    lattice_renderer_.Init(device, swap_chain, depth_format_);
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

    if (scene.dem != framed_dem_ && scene.lattice) {
      auto const& l = *scene.lattice;
      glm::vec3 shape{l.lattice_width, l.lattice_height, l.lattice_depth};
      glm::vec3 half = 0.5F * shape / std::max({shape.x, shape.y, shape.z});
      camera_.InitTopView(-half, half);
      framed_dem_ = scene.dem;
    }
    auto const simulation_signal =
        scene.lattice && scene.lattice->ready_signal > 0
            ? std::optional<std::uint64_t>(scene.lattice->ready_signal)
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
    vk::ImageMemoryBarrier2 depth_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                        vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask = depth_initialized_
                             ? vk::AccessFlagBits2::eDepthStencilAttachmentWrite
                             : vk::AccessFlags2{},
        .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                        vk::PipelineStageFlagBits2::eLateFragmentTests,
        .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                         vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .oldLayout = depth_initialized_
                         ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                         : vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = *depth_image_,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    command_buffer_.pipelineBarrier2(
        {.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depth_barrier});
    depth_initialized_ = true;
    vk::RenderingAttachmentInfo depth_attachment{
        .imageView = *depth_view_,
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .clearValue = vk::ClearDepthStencilValue(1.0F, 0)};
    vk::RenderingInfo rendering{.renderArea = {.offset = {.x = 0, .y = 0},
                                               .extent = swap_chain.Extent()},
                                .layerCount = 1,
                                .colorAttachmentCount = 1,
                                .pColorAttachments = &attachment,
                                .pDepthAttachment = &depth_attachment};
    command_buffer_.beginRendering(rendering);
    lattice_renderer_.Render(command_buffer_, swap_chain, scene.lattice,
                             camera_);
    command_buffer_.endRendering();
    // Overlay uses its existing color-only pipeline in a separate rendering
    // scope.
    vk::MemoryBarrier2 overlay_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead |
                         vk::AccessFlagBits2::eColorAttachmentWrite};
    command_buffer_.pipelineBarrier2(
        {.memoryBarrierCount = 1, .pMemoryBarriers = &overlay_barrier});
    attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    rendering.pDepthAttachment = nullptr;
    command_buffer_.beginRendering(rendering);
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

    vk::PipelineStageFlags wait_stage =
        vk::PipelineStageFlagBits::eVertexShader;
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
    lattice_renderer_.Shutdown();
    command_buffer_ = nullptr;
    depth_view_ = nullptr;
    depth_image_ = nullptr;
    depth_memory_ = nullptr;
  }

 private:
  void CreateDepth(graphics::Device const& device, vk::Extent2D extent) {
    depth_view_ = nullptr;
    depth_image_ = nullptr;
    depth_memory_ = nullptr;
    depth_initialized_ = false;
    for (auto format : {vk::Format::eD32Sfloat, vk::Format::eD16Unorm}) {
      if (device.PhysicalDevice()
              .getFormatProperties(format)
              .optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
        depth_format_ = format;
        break;
      }
    }
    if (depth_format_ == vk::Format::eUndefined)
      throw std::runtime_error("No supported depth format");
    depth_image_ = vk::raii::Image(
        device.LogicalDevice(),
        {.imageType = vk::ImageType::e2D,
         .format = depth_format_,
         .extent = {extent.width, extent.height, 1},
         .mipLevels = 1,
         .arrayLayers = 1,
         .samples = vk::SampleCountFlagBits::e1,
         .tiling = vk::ImageTiling::eOptimal,
         .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
         .sharingMode = vk::SharingMode::eExclusive});
    auto requirements = depth_image_.getMemoryRequirements();
    depth_memory_ = vk::raii::DeviceMemory(
        device.LogicalDevice(),
        {.allocationSize = requirements.size,
         .memoryTypeIndex = graphics::MemoryAllocator::FindMemoryType(
             device, requirements.memoryTypeBits,
             vk::MemoryPropertyFlagBits::eDeviceLocal)});
    depth_image_.bindMemory(depth_memory_, 0);
    depth_view_ = vk::raii::ImageView(
        device.LogicalDevice(),
        {.image = *depth_image_,
         .viewType = vk::ImageViewType::e2D,
         .format = depth_format_,
         .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                              .baseMipLevel = 0,
                              .levelCount = 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1}});
  }

  void RecreateSwapChain(graphics::Device const& device,
                         graphics::SwapChain& swap_chain,
                         platform::Window const& window,
                         IOverlayPass& overlay) {
    swap_chain.RecreateSwapChain(device, window);
    CreateDepth(device, swap_chain.Extent());
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

  vk::raii::DeviceMemory depth_memory_ = nullptr;
  vk::raii::Image depth_image_ = nullptr;
  vk::raii::ImageView depth_view_ = nullptr;
  vk::Format depth_format_ = vk::Format::eUndefined;
  bool depth_initialized_ = false;
  domain::SharedDem framed_dem_;
  vk::raii::CommandBuffer command_buffer_ = nullptr;
  LatticeRenderer lattice_renderer_;
  Camera camera_;
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
