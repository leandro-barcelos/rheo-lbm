#ifndef RHEO_RENDERER_RENDER_RESULT_H
#define RHEO_RENDERER_RENDER_RESULT_H

namespace renderer {

enum class RenderResult {
  kSuccess,
  kSwapChainOutOfDate,
};

}  // namespace renderer

#endif  // RHEO_RENDERER_RENDER_RESULT_H
