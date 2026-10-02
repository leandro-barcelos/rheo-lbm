#include "rheo/renderer/free_surface_renderer.h"

#include <algorithm>
#include <stdexcept>

#include "camera.h"
#include "lattice_renderer.h"
#include "rheo/graphics/images.h"

namespace renderer {

class FreeSurfaceRenderer::Impl {
 public:
  explicit Impl(platform::WindowSize initial_window_size)
      : camera_(glm::vec3(0.5F, 2.0F, 0.5F), initial_window_size) {}

  void Init(graphics::Device const& device,
            graphics::SwapChain const& swap_chain) {
    CreateDepth(device, swap_chain.Extent());
    lattice_renderer_.Init(device, swap_chain, depth_format_);
  }

  void PrepareCamera(application::SceneState const& scene,
                     platform::WindowSize size) {
    camera_.OnWindowResizedEvent({.width = size.width, .height = size.height});
    if (scene.lattice) {
      auto const& lattice = *scene.lattice;
      glm::vec3 shape{lattice.lattice_width, lattice.lattice_height,
                      lattice.lattice_depth};
      if (scene.dem == framed_dem_ && shape == framed_shape_) {
        return;
      }
      framed_shape_ = shape;
      glm::vec3 half = 0.5F * shape / std::max({shape[0], shape[1], shape[2]});
      camera_.InitTopView(-half, half);
      framed_dem_ = scene.dem;
    }
  }

  [[nodiscard]] domain::Ray ScreenRay(double xpos, double ypos) const {
    return camera_.ScreenRay(xpos, ypos);
  }

  void HandleInput(events::InputEvent const& event) {
    camera_.HandleInput(event);
  }

  vk::RenderingAttachmentInfo Prepare(vk::raii::CommandBuffer const& command,
                                      graphics::SwapChain const& swap_chain,
                                      application::SceneState const& scene) {
    auto const extent = swap_chain.Extent();
    PrepareCamera(scene,
                  {.width = int(extent.width), .height = int(extent.height)});
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
        .image = *depth_.Image(),
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    command.pipelineBarrier2(
        {.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depth_barrier});
    depth_initialized_ = true;
    return vk::RenderingAttachmentInfo{
        .imageView = *depth_.ImageView(),
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .clearValue = vk::ClearDepthStencilValue(1.0F, 0)};
  }

  void Render(vk::raii::CommandBuffer const& command,
              graphics::SwapChain const& swap_chain,
              application::SceneState const& scene) {
    lattice_renderer_.Render(command, swap_chain, scene.lattice, camera_,
                             scene.preview);
  }

  void OnSwapChainRecreated(graphics::Device const& device,
                            graphics::SwapChain const& swap_chain) {
    CreateDepth(device, swap_chain.Extent());
    auto const extent = swap_chain.Extent();
    camera_.OnWindowResizedEvent(
        {.width = int(extent.width), .height = int(extent.height)});
  }

  void Shutdown() {
    lattice_renderer_.Shutdown();
    depth_ = {};
  }

 private:
  void CreateDepth(graphics::Device const& device, vk::Extent2D extent) {
    depth_ = {};
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
    if (depth_format_ == vk::Format::eUndefined) {
      throw std::runtime_error("No supported depth format");
    }
    depth_ = graphics::ImageAllocator::CreateDepthImage(device, extent,
                                                        depth_format_);
  }

  graphics::AllocatedImage depth_;
  vk::Format depth_format_ = vk::Format::eUndefined;
  bool depth_initialized_ = false;
  domain::SharedDem framed_dem_;
  glm::vec3 framed_shape_{};
  LatticeRenderer lattice_renderer_;
  Camera camera_;
};

FreeSurfaceRenderer::FreeSurfaceRenderer(
    platform::WindowSize initial_window_size)
    : impl_(std::make_unique<Impl>(initial_window_size)) {}
FreeSurfaceRenderer::~FreeSurfaceRenderer() = default;

void FreeSurfaceRenderer::Init(graphics::Device const& device,
                               graphics::SwapChain const& swap_chain) {
  impl_->Init(device, swap_chain);
}
void FreeSurfaceRenderer::OnSwapChainRecreated(
    graphics::Device const& device, graphics::SwapChain const& swap_chain) {
  impl_->OnSwapChainRecreated(device, swap_chain);
}
void FreeSurfaceRenderer::PrepareCamera(application::SceneState const& scene,
                                        platform::WindowSize size) {
  impl_->PrepareCamera(scene, size);
}
domain::Ray FreeSurfaceRenderer::ScreenPointToRay(double xpos,
                                                  double ypos) const {
  return impl_->ScreenRay(xpos, ypos);
}
void FreeSurfaceRenderer::HandleInput(events::InputEvent const& event) {
  impl_->HandleInput(event);
}
void FreeSurfaceRenderer::Shutdown() { impl_->Shutdown(); }

FreeSurfaceRenderer::Pass FreeSurfaceRenderer::MakePass(
    application::SceneState const& scene) {
  return {*this, scene};
}
FreeSurfaceRenderer::Pass::Pass(FreeSurfaceRenderer& renderer,
                                application::SceneState const& scene)
    : renderer_(renderer), scene_(scene) {}
std::uint64_t FreeSurfaceRenderer::Pass::ReadySignal() const {
  return scene_.lattice ? scene_.lattice->ready_signal : 0;
}
std::optional<vk::RenderingAttachmentInfo> FreeSurfaceRenderer::Pass::Prepare(
    vk::raii::CommandBuffer const& command,
    graphics::SwapChain const& swap_chain) {
  return renderer_.impl_->Prepare(command, swap_chain, scene_);
}
void FreeSurfaceRenderer::Pass::Render(vk::raii::CommandBuffer const& command,
                                       graphics::SwapChain const& swap_chain) {
  renderer_.impl_->Render(command, swap_chain, scene_);
}

}  // namespace renderer
