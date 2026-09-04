#ifndef RHEO_GRAPHICS_COMMAND_LIST_H
#define RHEO_GRAPHICS_COMMAND_LIST_H

namespace graphics {

// Opaque view valid only for the duration of the call that provides it.
struct CommandList {
  void* native_handle = nullptr;
};

}  // namespace graphics

#endif  // RHEO_GRAPHICS_COMMAND_LIST_H
