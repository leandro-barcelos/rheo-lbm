#ifndef RHEO_RENDERER_SHALLOW_WATER_RENDERER_H
#define RHEO_RENDERER_SHALLOW_WATER_RENDERER_H

#include <memory>

#include "rheo/domain/shallow_water.h"
namespace renderer {

struct ShallowWaterMapOptions {
  int field = 0;
  bool vectors = true;
} __attribute__((aligned(8)));

class ShallowWaterRenderer {
 public:
  ShallowWaterRenderer();
  ShallowWaterRenderer(const ShallowWaterRenderer&) = delete;
  ShallowWaterRenderer(ShallowWaterRenderer&&) = delete;
  ShallowWaterRenderer& operator=(const ShallowWaterRenderer&) = delete;
  ShallowWaterRenderer& operator=(ShallowWaterRenderer&&) = delete;
  ~ShallowWaterRenderer();
  void DrawMap(const domain::ShallowWaterSnapshot&,
               const domain::ShallowWaterSettings&, ShallowWaterMapOptions);
  void FitMap();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace renderer

#endif  // RHEO_RENDERER_SHALLOW_WATER_RENDERER_H
