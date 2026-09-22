#include "rheo/renderer/shallow_water_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "imgui.h"
namespace renderer {
namespace {
ImU32 Color(double t) {
  t = std::clamp(t, 0., 1.);
  return ImGui::ColorConvertFloat4ToU32({float(std::clamp(2 * t - .3, 0., 1.)),
                                         float(std::sin(t * 3.141592653589793)),
                                         float(1 - t), 1});
}
}  // namespace
class ShallowWaterRenderer::Impl {
 public:
  float zoom_ = 1;
  ImVec2 pan_{};
  vk::raii::CommandBuffer cmd = nullptr;
  void DrawMap(const domain::ShallowWaterSnapshot& s,
               const domain::ShallowWaterSettings& p,
               ShallowWaterMapOptions options) {
    auto value = [&](int i) {
      return options.field == 0 ? s.depth[i]
             : options.field == 1
                 ? s.depth[i] + s.bed[i]
                 : std::hypot(s.velocity_x[i], s.velocity_y[i]);
    };
    double lo = std::numeric_limits<double>::infinity(), hi = -lo, speed = 0;
    for (int i = 0; i < p.nx * p.ny; ++i)
      if (!s.obstacles[i] && std::isfinite(value(i))) {
        lo = std::min(lo, value(i));
        hi = std::max(hi, value(i));
        const double magnitude = std::hypot(s.velocity_x[i], s.velocity_y[i]);
        if (std::isfinite(magnitude)) speed = std::max(speed, magnitude);
      }
    ImGui::Text("Range: %.6g to %.6g %s", lo, hi,
                options.field == 2 ? "m/s" : "m");
    auto* draw = ImGui::GetWindowDrawList();
    auto legend = ImGui::GetCursorScreenPos();
    for (int k = 0; k < 128; ++k)
      draw->AddRectFilled({legend.x + k * 2.f, legend.y},
                          {legend.x + (k + 1) * 2.f, legend.y + 10},
                          Color(k / 127.));
    ImGui::Dummy({256, 14});
    auto origin = ImGui::GetCursorScreenPos(),
         avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(1.f, avail.x);
    avail.y = std::max(1.f, avail.y);
    ImGui::InvisibleButton("Map canvas", avail,
                           ImGuiButtonFlags_MouseButtonRight);
    auto& io = ImGui::GetIO();
    if (ImGui::IsItemHovered()) {
      if (io.MouseWheel != 0) {
        float previous = zoom_;
        zoom_ = std::clamp(zoom_ * std::pow(1.15f, io.MouseWheel), .2f, 40.f);
        float ratio = zoom_ / previous;
        pan_.x = (pan_.x - (io.MousePos.x - origin.x - avail.x / 2)) * ratio +
                 (io.MousePos.x - origin.x - avail.x / 2);
        pan_.y = (pan_.y - (io.MousePos.y - origin.y - avail.y / 2)) * ratio +
                 (io.MousePos.y - origin.y - avail.y / 2);
      }
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        pan_.x += io.MouseDelta.x;
        pan_.y += io.MouseDelta.y;
      }
    }
    float cell = std::min(avail.x / p.nx, avail.y / p.ny) * zoom_;
    ImVec2 base{origin.x + (avail.x - cell * p.nx) / 2 + pan_.x,
                origin.y + (avail.y - cell * p.ny) / 2 + pan_.y};
    draw->PushClipRect(origin, {origin.x + avail.x, origin.y + avail.y}, true);
    draw->AddRectFilled(origin, {origin.x + avail.x, origin.y + avail.y},
                        IM_COL32(15, 20, 28, 255));
    for (int y = 0; y < p.ny; ++y)
      for (int x = 0; x < p.nx; ++x) {
        ImVec2 a{base.x + x * cell, base.y + (p.ny - 1 - y) * cell},
            b{a.x + cell, a.y + cell};
        if (b.x < origin.x || a.x > origin.x + avail.x || b.y < origin.y ||
            a.y > origin.y + avail.y)
          continue;
        int i = x + y * p.nx;
        ImU32 color = s.obstacles[i] ? IM_COL32(90, 94, 102, 255)
                      : !std::isfinite(value(i))
                          ? IM_COL32(255, 0, 255, 255)
                          : Color(hi > lo ? (value(i) - lo) / (hi - lo) : .5);
        draw->AddRectFilled(a, b, color);
      }
    if (options.vectors && speed > 1e-12) {
      int stride = std::max(1, int(std::ceil(20 / cell)));
      for (int y = std::min(stride / 2, (p.ny - 1) / 2); y < p.ny; y += stride)
        for (int x = std::min(stride / 2, (p.nx - 1) / 2); x < p.nx;
             x += stride) {
          int i = x + y * p.nx;
          if (s.obstacles[i] || !std::isfinite(s.velocity_x[i]) ||
              !std::isfinite(s.velocity_y[i]))
            continue;
          ImVec2 a{base.x + (x + .5f) * cell, base.y + (p.ny - y - .5f) * cell};
          float dx = float(s.velocity_x[i] / speed) * stride * cell * .8f,
                dy = -float(s.velocity_y[i] / speed) * stride * cell * .8f;
          ImVec2 b{a.x + dx, a.y + dy};
          draw->AddLine(a, b, IM_COL32(255, 255, 255, 230));
          draw->AddLine(b,
                        {b.x - .3f * dx + .2f * dy, b.y - .3f * dy - .2f * dx},
                        IM_COL32_WHITE);
          draw->AddLine(b,
                        {b.x - .3f * dx - .2f * dy, b.y - .3f * dy + .2f * dx},
                        IM_COL32_WHITE);
        }
    }
    draw->PopClipRect();
  }
  void RenderFrame(const graphics::Device& device, graphics::SwapChain& swap,
                   graphics::FrameSync& sync, const platform::Window& window,
                   IOverlayPass& overlay) {
    auto index = swap.AcquireNextImage(device, sync);
    if (index == graphics::SwapChain::kInvalidImageIndex) {
      swap.RecreateSwapChain(device, window);
      overlay.OnFrameResourcesChanged(swap.ImageCount());
      return;
    }
    cmd.reset();
    cmd.begin({});
    auto transition = [&](vk::ImageLayout old_layout,
                          vk::ImageLayout new_layout, bool finish) {
      vk::ImageMemoryBarrier2 barrier{
          .srcStageMask =
              finish ? vk::PipelineStageFlagBits2::eColorAttachmentOutput
                     : vk::PipelineStageFlagBits2::eTopOfPipe,
          .srcAccessMask = finish ? vk::AccessFlagBits2::eColorAttachmentWrite
                                  : vk::AccessFlags2{},
          .dstStageMask =
              finish ? vk::PipelineStageFlagBits2::eBottomOfPipe
                     : vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          .dstAccessMask = finish ? vk::AccessFlags2{}
                                  : vk::AccessFlagBits2::eColorAttachmentWrite,
          .oldLayout = old_layout,
          .newLayout = new_layout,
          .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
          .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
          .image = swap.GetImage(index),
          .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .levelCount = 1,
                               .layerCount = 1}};
      cmd.pipelineBarrier2(
          {.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
      swap.SetImageLayout(index, new_layout);
    };
    transition(swap.GetImageLayout(index),
               vk::ImageLayout::eColorAttachmentOptimal, false);
    vk::RenderingAttachmentInfo attachment{
        .imageView = swap.GetImageView(index),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f)};
    cmd.beginRendering(
        {.renderArea = {.offset = {0, 0}, .extent = swap.Extent()},
         .layerCount = 1,
         .colorAttachmentCount = 1,
         .pColorAttachments = &attachment});
    overlay.Render(graphics::CommandList{
        .native_handle = static_cast<VkCommandBuffer>(*cmd)});
    cmd.endRendering();
    transition(vk::ImageLayout::eColorAttachmentOptimal,
               vk::ImageLayout::ePresentSrcKHR, true);
    cmd.end();
    device.GraphicsQueue().submit(
        vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*cmd},
        nullptr);
    device.GraphicsQueue().waitIdle();
    auto result =
        device.PresentQueue().presentKHR({.swapchainCount = 1,
                                          .pSwapchains = &*swap.Handle(),
                                          .pImageIndices = &index});
    if (result == vk::Result::eSuboptimalKHR ||
        result == vk::Result::eErrorOutOfDateKHR) {
      swap.RecreateSwapChain(device, window);
      overlay.OnFrameResourcesChanged(swap.ImageCount());
    }
  }
};
ShallowWaterRenderer::ShallowWaterRenderer()
    : impl_(std::make_unique<Impl>()) {}
ShallowWaterRenderer::~ShallowWaterRenderer() = default;
void ShallowWaterRenderer::Init(const graphics::Device& device,
                                const graphics::CommandPools& pools) {
  vk::CommandBufferAllocateInfo allocation{
      .commandPool = *pools.Graphics(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  impl_->cmd = std::move(
      device.LogicalDevice().allocateCommandBuffers(allocation).front());
}
void ShallowWaterRenderer::FitMap() {
  impl_->zoom_ = 1;
  impl_->pan_ = {};
}
void ShallowWaterRenderer::DrawMap(const domain::ShallowWaterSnapshot& s,
                                   const domain::ShallowWaterSettings& p,
                                   ShallowWaterMapOptions options) {
  impl_->DrawMap(s, p, options);
}
void ShallowWaterRenderer::RenderFrame(const graphics::Device& d,
                                       graphics::SwapChain& s,
                                       graphics::FrameSync& f,
                                       const platform::Window& w,
                                       IOverlayPass& o) {
  impl_->RenderFrame(d, s, f, w, o);
}
}  // namespace renderer
