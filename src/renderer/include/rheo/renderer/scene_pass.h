#ifndef RHEO_RENDERER_SCENE_PASS_H
#define RHEO_RENDERER_SCENE_PASS_H

#include <cstdint>
#include <optional>

#include "rheo/graphics/swap_chain.h"

namespace renderer {

class IScenePass {
 public:
  IScenePass() = default;
  IScenePass(const IScenePass&) = default;
  IScenePass(IScenePass&&) = delete;
  IScenePass& operator=(const IScenePass&) = default;
  IScenePass& operator=(IScenePass&&) = delete;
  virtual ~IScenePass() = default;
  [[nodiscard]] virtual std::uint64_t ReadySignal() const = 0;
  virtual std::optional<vk::RenderingAttachmentInfo> Prepare(
      vk::raii::CommandBuffer const& command,
      graphics::SwapChain const& swap_chain) = 0;
  virtual void Render(vk::raii::CommandBuffer const& command,
                      graphics::SwapChain const& swap_chain) = 0;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_SCENE_PASS_H
