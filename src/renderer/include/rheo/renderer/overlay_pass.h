#ifndef RHEO_RENDERER_OVERLAY_PASS_H
#define RHEO_RENDERER_OVERLAY_PASS_H

#include <cstdint>

#include "rheo/graphics/command_list.h"

namespace renderer {

class IOverlayPass {
 public:
  virtual ~IOverlayPass() = default;
  virtual void Render(graphics::CommandList command_list) const = 0;
  virtual void OnFrameResourcesChanged(std::uint32_t image_count) = 0;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_OVERLAY_PASS_H
