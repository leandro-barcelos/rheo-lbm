#include "rheo/renderer/renderer.h"

#include <cassert>
#include <cstdint>
#include <utility>

namespace renderer {

class Renderer::Impl {
 public:
  void Init(graphics::Device const& device,
            graphics::CommandPools const& command_pools) {
    vk::CommandBufferAllocateInfo allocation{
        .commandPool = *command_pools.Graphics(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};
    command_buffer_ = std::move(
        device.LogicalDevice().allocateCommandBuffers(allocation).front());
  }

  RenderResult RenderFrame(graphics::Device const& device,
                           graphics::SwapChain& swap_chain,
                           graphics::FrameSync& frame_sync,
                           IOverlayPass const& overlay, IScenePass* scene) {
    auto const image_index = AcquireFrame(device, swap_chain, frame_sync);
    if (image_index == graphics::SwapChain::kInvalidImageIndex) {
      return RenderResult::kSwapChainOutOfDate;
    }

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

    if (scene != nullptr) {
      auto depth = scene->Prepare(command_buffer_, swap_chain);
      BeginColorPass(swap_chain, image_index, vk::AttachmentLoadOp::eClear,
                     depth ? &*depth : nullptr);
      scene->Render(command_buffer_, swap_chain);
      command_buffer_.endRendering();
      // ImGui's pipeline is color-only. Load the scene's color in a separate
      // rendering scope, with a barrier making the previous writes visible.
      vk::MemoryBarrier2 overlay_barrier{
          .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
          .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead |
                           vk::AccessFlagBits2::eColorAttachmentWrite};
      command_buffer_.pipelineBarrier2(
          {.memoryBarrierCount = 1, .pMemoryBarriers = &overlay_barrier});
    }

    BeginColorPass(swap_chain, image_index,
                   (scene != nullptr) ? vk::AttachmentLoadOp::eLoad
                                      : vk::AttachmentLoadOp::eClear);
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

    Submit(device, frame_sync, (scene != nullptr) ? scene->ReadySignal() : 0);
    return Present(device, swap_chain, image_index);
  }

  void Shutdown() { command_buffer_ = nullptr; }

 private:
  static std::uint32_t AcquireFrame(graphics::Device const& device,
                                    graphics::SwapChain& swap_chain,
                                    graphics::FrameSync& frame_sync) {
    return swap_chain.AcquireNextImage(device, frame_sync);
  }

  void BeginColorPass(graphics::SwapChain const& swap_chain,
                      std::uint32_t image_index, vk::AttachmentLoadOp load_op,
                      vk::RenderingAttachmentInfo const* depth = nullptr) {
    vk::RenderingAttachmentInfo attachment{
        .imageView = swap_chain.GetImageView(image_index),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = load_op,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F)};
    command_buffer_.beginRendering(
        {.renderArea = {.offset = {.x = 0, .y = 0},
                        .extent = swap_chain.Extent()},
         .layerCount = 1,
         .colorAttachmentCount = 1,
         .pColorAttachments = &attachment,
         .pDepthAttachment = depth});
  }

  void Submit(graphics::Device const& device, graphics::FrameSync& frame_sync,
              std::uint64_t wait_value) {
    std::uint64_t const signal_value = frame_sync.GetNextTimelineValue();
    vk::PipelineStageFlags wait_stage =
        vk::PipelineStageFlagBits::eVertexShader;
    vk::TimelineSemaphoreSubmitInfo timeline{
        .waitSemaphoreValueCount = wait_value > 0 ? 1U : 0U,
        .pWaitSemaphoreValues = wait_value > 0 ? &wait_value : nullptr,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &signal_value};
    vk::SubmitInfo submit{
        .pNext = &timeline,
        .waitSemaphoreCount = wait_value > 0 ? 1U : 0U,
        .pWaitSemaphores = wait_value > 0 ? &*frame_sync.Semaphore() : nullptr,
        .pWaitDstStageMask = wait_value > 0 ? &wait_stage : nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer_,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*frame_sync.Semaphore()};
    device.GraphicsQueue().submit(submit, nullptr);
    frame_sync.WaitSemaphore(device, signal_value);
  }

  static RenderResult Present(graphics::Device const& device,
                       graphics::SwapChain const& swap_chain,
                       std::uint32_t image_index) {
    vk::PresentInfoKHR present{.swapchainCount = 1,
                               .pSwapchains = &*swap_chain.Handle(),
                               .pImageIndices = &image_index};
    vk::Result result = vk::Result::eSuccess;
    try {
      result = device.PresentQueue().presentKHR(present);
    } catch (vk::OutOfDateKHRError const&) {
      return RenderResult::kSwapChainOutOfDate;
    }
    if (result == vk::Result::eSuboptimalKHR ||
        result == vk::Result::eErrorOutOfDateKHR) {
      return RenderResult::kSwapChainOutOfDate;
    }
    assert(result == vk::Result::eSuccess);
    return RenderResult::kSuccess;
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
};

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() = default;

void Renderer::Init(graphics::Device const& device,
                    graphics::CommandPools const& command_pools) {
  impl_->Init(device, command_pools);
}
RenderResult Renderer::RenderFrame(graphics::Device const& device,
                                   graphics::SwapChain& swap_chain,
                                   graphics::FrameSync& frame_sync,
                                   IOverlayPass const& overlay,
                                   IScenePass* scene) {
  return impl_->RenderFrame(device, swap_chain, frame_sync, overlay, scene);
}
void Renderer::Shutdown() { impl_->Shutdown(); }

}  // namespace renderer
