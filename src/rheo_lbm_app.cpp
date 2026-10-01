#include "rheo_lbm_app.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <variant>

#ifdef _OPENMP
#include <omp.h>
#endif

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
            simulation_.emplace(backend_.Device(), backend_.CommandPools(),
                                backend_.FrameSync());
            return decltype(application_){
                std::in_place_type<application::ApplicationController>,
                dem_loader_, image_loader_, project_repository_, *simulation_};
          case SimulationModel::kShallowWater2D:
#ifdef _OPENMP
            if (std::getenv("OMP_NUM_THREADS") == nullptr) {  // NOLINT
              omp_set_num_threads(std::min(4, omp_get_max_threads()));
            }
#endif
            return decltype(application_){
                std::in_place_type<application::ShallowWaterController>};
        }
        throw std::invalid_argument("Invalid simulation model");
      }()),
      renderer_([model, this]() -> decltype(renderer_) {
        switch (model) {
          case SimulationModel::kFreeSurface3D:
            return decltype(renderer_){std::in_place_type<renderer::Renderer>,
                                       backend_.Window().Size()};
          case SimulationModel::kShallowWater2D:
            return decltype(renderer_){
                std::in_place_type<renderer::ShallowWaterRenderer>};
        }
        throw std::invalid_argument("Invalid simulation model");
      }()) {
  if (auto* renderer = std::get_if<renderer::Renderer>(&renderer_)) {
    renderer->Init(backend_.Device(), backend_.SwapChain(),
                   backend_.CommandPools());
  } else {
    std::get<renderer::ShallowWaterRenderer>(renderer_).Init(
        backend_.Device(), backend_.CommandPools());
  }
  ui_.Init(backend_.Window(), backend_.GraphicsContext(), backend_.Device(),
           backend_.SwapChain(), model_ == SimulationModel::kFreeSurface3D);
  last_time_ = platform::Window::TimeSeconds();
}

RheoLBMApp::~RheoLBMApp() {
  backend_.WaitIdle();
  ui_.Shutdown();
  if (auto* renderer = std::get_if<renderer::Renderer>(&renderer_)) {
    renderer->Shutdown();
  }
}

void RheoLBMApp::Run() {
  auto* free_surface =
      std::get_if<application::ApplicationController>(&application_);
  auto* shallow_water =
      std::get_if<application::ShallowWaterController>(&application_);
  bool swap_chain_out_of_date = false;

  while (!backend_.ShouldClose() &&
         ((free_surface == nullptr) || !free_surface->ShouldQuit())) {
    runtime::ApplicationBackend::PollEvents();
    if (backend_.ShouldClose()) {
      break;
    }
    if (backend_.IsMinimized()) {
      runtime::ApplicationBackend::WaitEvents();
      last_time_ = platform::Window::TimeSeconds();
      continue;
    }

    UpdateDeltaTime();

    if (swap_chain_out_of_date || backend_.NeedsSwapChainRecreation()) {
      swap_chain_out_of_date = true;
      if (!backend_.RecreateSwapChain()) {
        continue;
      }
      if (auto* renderer = std::get_if<renderer::Renderer>(&renderer_)) {
        renderer->OnSwapChainRecreated(backend_.Device(), backend_.SwapChain());
      }
      ui_.OnFrameResourcesChanged(backend_.SwapChain().ImageCount());
      swap_chain_out_of_date = false;
    }

    if (shallow_water != nullptr) {
      (void)backend_.InputQueue().Drain();
      shallow_water->Update();
      backend_.WaitIdle();
    }

    ui_.BeginFrame();
    if (free_surface != nullptr) {
      ui_.Draw(free_surface->ViewState(), *free_surface);
    } else {
      ui_.Draw(*shallow_water,
               std::get<renderer::ShallowWaterRenderer>(renderer_));
    }
    ui_.EndFrame();

    renderer::RenderResult result;
    if (free_surface != nullptr) {
      auto& renderer = std::get<renderer::Renderer>(renderer_);
      free_surface->ProcessPendingCommands();
      renderer.PrepareCamera(free_surface->SceneState(),
                             backend_.Window().Size());
      RouteInput(*free_surface, renderer, ui::UserInterface::InputCapture());
      free_surface->ProcessPendingCommands();
      if (free_surface->ShouldQuit()) {
        break;
      }
      free_surface->Update(delta_time_);
      result = renderer.RenderFrame(backend_.Device(), backend_.SwapChain(),
                                    backend_.FrameSync(),
                                    free_surface->SceneState(), ui_);
    } else {
      result = std::get<renderer::ShallowWaterRenderer>(renderer_).RenderFrame(
          backend_.Device(), backend_.SwapChain(), backend_.FrameSync(), ui_);
    }
    swap_chain_out_of_date =
        result == renderer::RenderResult::kSwapChainOutOfDate;
  }
}

void RheoLBMApp::UpdateDeltaTime() {
  double const current_time = platform::Window::TimeSeconds();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}

void RheoLBMApp::RouteInput(application::ApplicationController& application,
                            renderer::Renderer& renderer,
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
