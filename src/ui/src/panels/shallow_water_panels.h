#ifndef RHEO_UI_SHALLOW_WATER_PANELS_H
#define RHEO_UI_SHALLOW_WATER_PANELS_H

#include "rheo/application/shallow_water_controller.h"
#include "rheo/renderer/shallow_water_renderer.h"

namespace ui {

class ShallowWaterPanels {
 public:
  void Draw(application::ShallowWaterController& controller,
            renderer::ShallowWaterRenderer& map);

 private:
  renderer::ShallowWaterMapOptions options_;
};

}  // namespace ui

#endif  // RHEO_UI_SHALLOW_WATER_PANELS_H
