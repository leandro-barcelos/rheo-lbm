#include "free_surface_app.h"

#include "rheo/runtime/application_backend.h"

namespace rheo {
FreeSurfaceApp::FreeSurfaceApp(runtime::ApplicationBackend& backend)
    : simulation_(backend.Device(), backend.CommandPools(),
                  backend.FrameSync()),
      application_(dem_loader_, image_loader_, project_repository_,
                   simulation_),
      renderer_(backend.Window().Size()) {
  renderer_.Init(backend.Device(), backend.SwapChain(), backend.CommandPools());
}

FreeSurfaceApp::~FreeSurfaceApp() { renderer_.Shutdown(); }

bool FreeSurfaceApp::Update(runtime::ApplicationBackend& backend,
                            double delta_time,
                            ui::UserInterface& user_interface) {
  user_interface.BeginFrame();
  user_interface.Draw(application_.ViewState(), application_);
  user_interface.EndFrame();
  application_.ProcessPendingCommands();
  renderer_.PrepareCamera(application_.SceneState(), backend.Window().Size());
  RouteInput(backend, ui::UserInterface::InputCapture());

  application_.ProcessPendingCommands();
  if (application_.ShouldQuit()) {
    return true;
  }
  application_.Update(delta_time);
  renderer_.RenderFrame(backend.Device(), backend.SwapChain(),
                        backend.FrameSync(), application_.SceneState(),
                        backend.Window(), user_interface);

  return application_.ShouldQuit() || backend.ShouldClose();
}

void FreeSurfaceApp::RouteInput(runtime::ApplicationBackend& backend,
                                ui::InputCaptureState capture) {
  auto logical = backend.Window().LogicalSize();
  auto pixels = backend.Window().Size();
  auto events = backend.InputQueue().Drain();
  editor_input_.Route(
      events, {.mouse = capture.mouse, .keyboard = capture.keyboard},
      {.width = logical.width, .height = logical.height},
      {.width = pixels.width, .height = pixels.height}, application_,
      {.ray =
           [this](double xpos, double ypos) {
             return renderer_.ScreenPointToRay(xpos, ypos);
           },
       .camera =
           [this](events::InputEvent const& event) {
             renderer_.HandleInput(event);
           },
       .resize = [this] { renderer_.RequestResize(); }});
}

}  // namespace rheo
