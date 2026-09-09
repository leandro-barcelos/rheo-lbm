#ifndef RHEOLBM_MEMORY_H
#define RHEOLBM_MEMORY_H

#include <vk_mem_alloc.h>

namespace graphics {

class GraphicsContext;
class Device;

class MemoryAllocator {
 public:
  MemoryAllocator(const MemoryAllocator&) = delete;
  MemoryAllocator(MemoryAllocator&&) = delete;
  MemoryAllocator& operator=(const MemoryAllocator&) = delete;
  MemoryAllocator& operator=(MemoryAllocator&&) = delete;
  MemoryAllocator() = default;
  ~MemoryAllocator();

  void Init(graphics::GraphicsContext const& context,
            graphics::Device const& device);
  [[nodiscard]] VmaAllocator Allocator() const { return allocator_; }

 private:
  VmaAllocator allocator_{};
};

}  // namespace graphics

#endif  // !RHEOLBM_MEMORY_H
