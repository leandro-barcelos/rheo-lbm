#ifndef RHEO_APPLICATION_APPLICATION_COMMAND_H
#define RHEO_APPLICATION_APPLICATION_COMMAND_H

#include <string>
#include <variant>

#include "rheo/application/lattice_brush_controller.h"
#include "rheo/domain/simulation_config.h"

namespace application {

struct UpdateSimulationDraft {
  domain::SimulationSettingsDraft draft;
};
struct ImportTerrain {
  std::string path;
};
struct SetTerrainTexture {
  std::string path;
};
struct LoadProject {
  std::string path;
};
struct SaveProject {
  std::string path;
};
struct NewProject {};
struct PlaySimulation {};
struct PauseSimulation {};
struct ResetSimulation {};
struct RequestQuit {};
struct SetBrush {
  domain::BrushSettings settings;
};
struct RunEditorAction {
  EditorAction action;
};
struct ConfirmDiscard {
  bool confirm;
};

using ApplicationCommand =
    std::variant<UpdateSimulationDraft, ImportTerrain, SetTerrainTexture,
                 LoadProject, SaveProject, NewProject, PlaySimulation,
                 PauseSimulation, ResetSimulation, RequestQuit, SetBrush,
                 BrushPointer, RunEditorAction, ConfirmDiscard>;

class ICommandSink {
 public:
  virtual ~ICommandSink() = default;
  virtual void Submit(ApplicationCommand command) = 0;
};

}  // namespace application

#endif  // RHEO_APPLICATION_APPLICATION_COMMAND_H
