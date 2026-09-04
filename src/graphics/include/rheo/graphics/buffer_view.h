#ifndef RHEO_GRAPHICS_BUFFER_VIEW_H
#define RHEO_GRAPHICS_BUFFER_VIEW_H

#include <cstdint>

namespace graphics {

// Opaque non-owning handle. Its lifetime is controlled by the producing module.
struct GraphicsBufferView {
  std::uintptr_t native_handle = 0;

  [[nodiscard]] explicit operator bool() const { return native_handle != 0; }
};

}  // namespace graphics

#endif  // RHEO_GRAPHICS_BUFFER_VIEW_H
