#ifndef RHEO_RENDERER_OVERLAY_PASS_H
#define RHEO_RENDERER_OVERLAY_PASS_H

#include "rheo/graphics/command_list.h"

namespace renderer {

class IOverlayPass {
 public:
  virtual ~IOverlayPass() = default;
  virtual void Render(graphics::CommandList command_list) const = 0;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_OVERLAY_PASS_H
