#ifndef RHEO_FREE_SURFACE_APP_H
#define RHEO_FREE_SURFACE_APP_H

#include "rheo/application/application_controller.h"
#include "rheo/application/editor_input_router.h"
#include "rheo/assets/asset_services.h"
#include "rheo/renderer/renderer.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/simulation/simulation_session.h"
#include "rheo/ui/user_interface.h"

namespace rheo {

class FreeSurfaceApp {
 public:
  FreeSurfaceApp(const FreeSurfaceApp&) = delete;
  FreeSurfaceApp(FreeSurfaceApp&&) = delete;
  FreeSurfaceApp& operator=(const FreeSurfaceApp&) = delete;
  FreeSurfaceApp& operator=(FreeSurfaceApp&&) = delete;
  explicit FreeSurfaceApp(runtime::ApplicationBackend& backend);
  ~FreeSurfaceApp();

  bool Update(runtime::ApplicationBackend& backend, double delta_time);

 private:
  void RouteInput(runtime::ApplicationBackend& backend,
                  ui::InputCaptureState capture);

  assets::DemLoader dem_loader_;
  assets::ImageLoader image_loader_;
  assets::ProjectRepository project_repository_;
  simulation::FreeSurfaceSession simulation_;
  application::ApplicationController application_;
  renderer::Renderer renderer_;
  ui::UserInterface ui_;
  application::EditorInputRouter editor_input_;
};

}  // namespace rheo

#endif  // RHEO_FREE_SURFACE_APP_H
