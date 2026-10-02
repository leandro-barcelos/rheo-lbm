#ifndef RHEO_LBM_APP_H
#define RHEO_LBM_APP_H

#include <optional>
#include <variant>

#include "rheo/application/application_controller.h"
#include "rheo/application/editor_input_router.h"
#include "rheo/application/shallow_water_controller.h"
#include "rheo/assets/asset_services.h"
#include "rheo/renderer/render_result.h"
#include "rheo/renderer/renderer.h"
#include "rheo/renderer/shallow_water_renderer.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/simulation/simulation_session.h"
#include "rheo/ui/user_interface.h"
#include "simulation_model.h"

namespace rheo {

class RheoLBMApp {
 public:
  explicit RheoLBMApp(SimulationModel model = SimulationModel::kFreeSurface3D);
  ~RheoLBMApp();
  RheoLBMApp(RheoLBMApp&&) = delete;
  RheoLBMApp& operator=(RheoLBMApp&&) = delete;
  RheoLBMApp(const RheoLBMApp&) = delete;
  RheoLBMApp& operator=(const RheoLBMApp&) = delete;
  void Run();

 private:
  using Application = std::variant<application::ApplicationController,
                                   application::ShallowWaterController>;
  using Renderer =
      std::variant<renderer::Renderer, renderer::ShallowWaterRenderer>;

  Application InitializeApplication();
  Renderer InitializeRenderer();
  [[nodiscard]] bool ShouldQuit() const;
  void UpdateDeltaTime();
  bool RecreateSwapChainIfNeeded(bool swap_chain_out_of_date);
  void DrawUi();
  void ProcessFreeSurfaceInput(application::ApplicationController& application);
  renderer::RenderResult RenderFrame();
  void RouteInput(application::ApplicationController& application,
                  renderer::Renderer& renderer, ui::InputCaptureState capture);

  runtime::ApplicationBackend backend_;
  SimulationModel model_;
  assets::DemLoader dem_loader_;
  assets::ImageLoader image_loader_;
  assets::ProjectRepository project_repository_;
  std::optional<simulation::FreeSurfaceSession> simulation_;
  Application application_;
  Renderer renderer_;
  ui::UserInterface ui_;
  application::EditorInputRouter editor_input_;
  double last_time_ = 0.0;
  double delta_time_ = 0.0;
};
}  // namespace rheo

#endif  // RHEO_LBM_APP_H
