#include "rheo_lbm_app.h"

#include <stdexcept>
#include <variant>

namespace rheo {

RheoLBMApp::RheoLBMApp(SimulationModel model)
    : backend_(runtime::ApplicationBackend::Config{
          .window_properties =
              platform::WindowProperties{
                  .width = 1280, .height = 720, .title = "Rheo LBM"}}),
      model_(model),
      application_([model, this]() -> decltype(application_) {
        switch (model) {
          case SimulationModel::kFreeSurface3D:
            return decltype(application_){std::in_place_type<FreeSurfaceApp>,
                                          backend_};
          case SimulationModel::kShallowWater2D:
            return decltype(application_){std::in_place_type<ShallowWaterApp>,
                                          backend_};
        }
        throw std::invalid_argument("Invalid simulation model");
      }()),
      last_time_(platform::Window::TimeSeconds()) {}

RheoLBMApp::~RheoLBMApp() { backend_.WaitIdle(); }

void RheoLBMApp::Run() {
  auto should_quit = false;
  while (!should_quit) {
    runtime::ApplicationBackend::PollEvents();
    if (backend_.IsMinimized()) {
      runtime::ApplicationBackend::WaitEvents();
      continue;
    }

    UpdateDeltaTime();

    std::visit(
        [&should_quit, this](auto& app) {
          should_quit = app.Update(backend_, delta_time_);
        },
        application_);
  }
}

void RheoLBMApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}

}  // namespace rheo
