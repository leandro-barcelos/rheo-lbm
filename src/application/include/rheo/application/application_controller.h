#ifndef RHEO_APPLICATION_APPLICATION_CONTROLLER_H
#define RHEO_APPLICATION_APPLICATION_CONTROLLER_H

#include <vector>

#include "rheo/application/application_command.h"
#include "rheo/application/application_state.h"
#include "rheo/assets/asset_services.h"
#include "rheo/simulation/simulation_session.h"

namespace application {

class ApplicationController final : public ICommandSink {
 public:
  ApplicationController(assets::IDemLoader const& dem_loader,
                        assets::IImageLoader const& image_loader,
                        assets::IProjectRepository const& project_repository,
                        simulation::ISimulationSession& simulation);

  void Submit(ApplicationCommand command) override;
  void ProcessPendingCommands();
  void Update(double delta_ms);

  [[nodiscard]] ApplicationViewState const& ViewState() const {
    return view_state_;
  }
  [[nodiscard]] application::SceneState const& SceneState() const {
    return scene_state_;
  }
  [[nodiscard]] bool ShouldQuit() const { return should_quit_; }

 private:
  void Handle(UpdateSimulationDraft const& command);
  void Handle(ImportTerrain const& command);
  void Handle(SetTerrainTexture const& command);
  void Handle(LoadProject const& command);
  void Handle(SaveProject const& command);
  void Handle(NewProject const& command);
  void Handle(PlaySimulation const& command);
  void Handle(PauseSimulation const& command);
  void Handle(ResetSimulation const& command);
  void Handle(RemoveDam const& command);
  void Handle(RequestQuit const& command);
  void Handle(SetBrush const& command);
  void Handle(BrushPointer const& command);
  void Handle(RunEditorAction const& command);
  void Handle(ConfirmDiscard const& command);
  void RefreshEditor();
  void ResetEditor();
  void RefreshSimulationConfig();
  void SetError(std::string message);

  assets::IDemLoader const& dem_loader_;
  assets::IImageLoader const& image_loader_;
  assets::IProjectRepository const& project_repository_;
  simulation::ISimulationSession& simulation_;
  LatticeBrushController brush_;
  std::optional<domain::SimulationSettingsDraft> pending_draft_;
  std::vector<ApplicationCommand> commands_;
  ApplicationViewState view_state_;
  application::SceneState scene_state_;
  bool should_quit_ = false;
};

}  // namespace application

#endif  // RHEO_APPLICATION_APPLICATION_CONTROLLER_H
