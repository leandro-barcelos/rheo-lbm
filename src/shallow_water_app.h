#ifndef RHEO_SHALLOW_WATER_APP_H
#define RHEO_SHALLOW_WATER_APP_H

#include "rheo/application/shallow_water_controller.h"
#include "rheo/renderer/shallow_water_renderer.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/ui/shallow_water_interface.h"

namespace rheo {

class ShallowWaterApp {
 public:
  ShallowWaterApp(const ShallowWaterApp&) = delete;
  ShallowWaterApp(ShallowWaterApp&&) = delete;
  ShallowWaterApp& operator=(const ShallowWaterApp&) = delete;
  ShallowWaterApp& operator=(ShallowWaterApp&&) = delete;
  explicit ShallowWaterApp(runtime::ApplicationBackend& backend);
  ~ShallowWaterApp();

  void Init(runtime::ApplicationBackend& backend);
  bool Update(runtime::ApplicationBackend& backend, double delta_time);

 private:
  application::ShallowWaterController application_;
  renderer::ShallowWaterRenderer renderer_;
  ui::ShallowWaterInterface ui_;
};

}  // namespace rheo

#endif  // RHEO_SHALLOW_WATER_APP_H
