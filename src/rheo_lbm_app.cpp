#include "rheo_lbm_app.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "rheo/renderer/render_result.h"
#include "rheo/runtime/application_backend.h"
#include "rheo/simulation/free_surface_session.h"
#include "simulation_model.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

std::optional<simulation::FreeSurfaceSession> InitializeSimulation(
    rheo::SimulationModel model, runtime::ApplicationBackend& backend) {
  return model == rheo::SimulationModel::kFreeSurface3D
             ? std::optional<
                   simulation::FreeSurfaceSession>{std::in_place,
                                                   backend.Device(),
                                                   backend.CommandPools(),
                                                   backend.FrameSync()}
             : std::nullopt;
}

}  // namespace

namespace rheo {

RheoLBMApp::RheoLBMApp(SimulationModel model)
    : backend_(runtime::ApplicationBackend::Config{
          .window_properties =
              platform::WindowProperties{
                  .width = 1280, .height = 720, .title = "Rheo LBM"}}),
      model_(model),
      simulation_(InitializeSimulation(model, backend_)),
      application_(InitializeApplication()) {
  renderer_.Init(backend_.Device(), backend_.CommandPools());
  if (model_ == SimulationModel::kFreeSurface3D) {
    free_surface_renderer_.emplace(backend_.Window().Size());
    free_surface_renderer_->Init(backend_.Device(), backend_.SwapChain());
  }
  ui_.Init(backend_.Window(), backend_.GraphicsContext(), backend_.Device(),
           backend_.SwapChain(), model_ == SimulationModel::kFreeSurface3D);
  last_time_ = platform::Window::TimeSeconds();
}

RheoLBMApp::~RheoLBMApp() {
  backend_.WaitIdle();
  ui_.Shutdown();
  renderer_.Shutdown();
  if (free_surface_renderer_) {
    free_surface_renderer_->Shutdown();
  }
}

void RheoLBMApp::Run() {
  auto* free_surface =
      std::get_if<application::ApplicationController>(&application_);
  auto* shallow_water =
      std::get_if<application::ShallowWaterController>(&application_);
  bool swap_chain_out_of_date = false;

  while (!ShouldQuit()) {
    runtime::ApplicationBackend::PollEvents();
    if (ShouldQuit()) {
      break;
    }

    if (backend_.IsMinimized()) {
      runtime::ApplicationBackend::WaitEvents();
      last_time_ = platform::Window::TimeSeconds();
      continue;
    }

    UpdateDeltaTime();
    swap_chain_out_of_date = RecreateSwapChainIfNeeded(swap_chain_out_of_date);

    if (swap_chain_out_of_date) {
      continue;
    }

    if (shallow_water != nullptr) {
      (void)backend_.InputQueue().Drain();
      shallow_water->Update();
    }

    DrawUi();

    if (free_surface != nullptr) {
      ProcessFreeSurfaceInput(*free_surface);
      if (ShouldQuit()) {
        break;
      }
      free_surface->Update(delta_time_);
    }
    swap_chain_out_of_date =
        RenderFrame() == renderer::RenderResult::kSwapChainOutOfDate;
  }
}

RheoLBMApp::Application RheoLBMApp::InitializeApplication() {
  switch (model_) {
    case SimulationModel::kFreeSurface3D:
      return Application{std::in_place_type<application::ApplicationController>,
                         dem_loader_, image_loader_, project_repository_,
                         *simulation_};
    case SimulationModel::kShallowWater2D:
#ifdef _OPENMP
      if (std::getenv("OMP_NUM_THREADS") == nullptr) {  // NOLINT
        omp_set_num_threads(std::min(4, omp_get_max_threads()));
      }
#endif
      return Application{
          std::in_place_type<application::ShallowWaterController>};
  }
  throw std::invalid_argument("Invalid simulation model");
}

bool RheoLBMApp::ShouldQuit() const {
  auto const* free_surface =
      std::get_if<application::ApplicationController>(&application_);
  return backend_.ShouldClose() ||
         (free_surface != nullptr && free_surface->ShouldQuit());
}

void RheoLBMApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}

bool RheoLBMApp::RecreateSwapChainIfNeeded(bool swap_chain_out_of_date) {
  if (swap_chain_out_of_date || backend_.NeedsSwapChainRecreation()) {
    if (!backend_.RecreateSwapChain()) {
      return true;
    }
    if (free_surface_renderer_) {
      free_surface_renderer_->OnSwapChainRecreated(backend_.Device(),
                                                   backend_.SwapChain());
    }
    ui_.OnFrameResourcesChanged(backend_.SwapChain().ImageCount());
  }

  return false;
}

void RheoLBMApp::DrawUi() {
  ui_.BeginFrame();
  if (auto* free_surface =
          std::get_if<application::ApplicationController>(&application_)) {
    ui_.Draw(free_surface->ViewState(), *free_surface);
  } else {
    ui_.Draw(std::get<application::ShallowWaterController>(application_));
  }
  ui_.EndFrame();
}

void RheoLBMApp::ProcessFreeSurfaceInput(
    application::ApplicationController& application) {
  auto& renderer = free_surface_renderer_.value();
  application.ProcessPendingCommands();
  renderer.PrepareCamera(application.SceneState(), backend_.Window().Size());
  RouteInput(application, renderer, ui::UserInterface::InputCapture());
  application.ProcessPendingCommands();
}

renderer::RenderResult RheoLBMApp::RenderFrame() {
  if (auto const* free_surface =
          std::get_if<application::ApplicationController>(&application_)) {
    auto pass = free_surface_renderer_->MakePass(free_surface->SceneState());
    return renderer_.RenderFrame(backend_.Device(), backend_.SwapChain(),
                                 backend_.FrameSync(), ui_, &pass);
  }
  return renderer_.RenderFrame(backend_.Device(), backend_.SwapChain(),
                               backend_.FrameSync(), ui_);
}

void RheoLBMApp::RouteInput(application::ApplicationController& application,
                            renderer::FreeSurfaceRenderer& renderer,
                            ui::InputCaptureState capture) {
  auto logical = backend_.Window().LogicalSize();
  auto pixels = backend_.Window().Size();
  auto events = backend_.InputQueue().Drain();
  editor_input_.Route(
      events, {.mouse = capture.mouse, .keyboard = capture.keyboard},
      {.width = logical.width, .height = logical.height},
      {.width = pixels.width, .height = pixels.height}, application,
      {.ray =
           [&renderer](double xpos, double ypos) {
             return renderer.ScreenPointToRay(xpos, ypos);
           },
       .camera =
           [&renderer](events::InputEvent const& event) {
             renderer.HandleInput(event);
           }});
}

}  // namespace rheo
