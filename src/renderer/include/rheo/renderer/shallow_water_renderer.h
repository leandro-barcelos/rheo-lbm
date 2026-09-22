#pragma once
#include <memory>

#include "rheo/domain/shallow_water.h"
#include "rheo/graphics/command_pool.h"
#include "rheo/graphics/frame_sync.h"
#include "rheo/graphics/swap_chain.h"
#include "rheo/platform/window.h"
#include "rheo/renderer/overlay_pass.h"
namespace renderer {
struct ShallowWaterMapOptions {
  int field = 0;
  bool vectors = true;
};
class ShallowWaterRenderer {
 public:
  ShallowWaterRenderer();
  ~ShallowWaterRenderer();
  void Init(const graphics::Device&, const graphics::CommandPools&);
  void DrawMap(const domain::ShallowWaterSnapshot&,
               const domain::ShallowWaterSettings&, ShallowWaterMapOptions);
  void FitMap();
  void RenderFrame(const graphics::Device&, graphics::SwapChain&,
                   graphics::FrameSync&, const platform::Window&,
                   IOverlayPass&);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace renderer
