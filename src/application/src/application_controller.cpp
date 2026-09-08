#include "rheo/application/application_controller.h"

#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <variant>

namespace {

std::string EnsureProjectExtension(std::string path) {
  if (path.empty()) {
    return path;
  }
  auto const extension = std::filesystem::path(path).extension().string();
  if (extension != ".yaml" && extension != ".yml") {
    path += ".yaml";
  }
  return path;
}

}  // namespace

application::ApplicationController::ApplicationController(
    assets::IDemLoader const& dem_loader,
    assets::IImageLoader const& image_loader,
    assets::IProjectRepository const& project_repository,
    simulation::ISimulationSession& simulation)
    : dem_loader_(dem_loader),
      image_loader_(image_loader),
      project_repository_(project_repository),
      simulation_(simulation),
      brush_(simulation) {}

void application::ApplicationController::Submit(ApplicationCommand command) {
  commands_.push_back(std::move(command));
}

void application::ApplicationController::ProcessPendingCommands() {
  auto commands = std::move(commands_);
  commands_.clear();
  for (auto const& command : commands) {
    std::visit([this](auto const& value) { Handle(value); }, command);
  }
}

void application::ApplicationController::Update(double delta_ms) {
  try {
    scene_state_.lattice = simulation_.Update(delta_ms);
  } catch (std::exception const& error) {
    simulation_.Pause();
    SetError(std::string("LBM step failed: ") + error.what());
  }
  view_state_.simulation_running = simulation_.IsRunning();
  view_state_.simulation_paused =
      simulation_.State() == simulation::SimulationState::kPaused;
  view_state_.physical_step_count = simulation_.PhysicalStepCount();
  RefreshSimulationConfig();
  RefreshEditor();
}

void application::ApplicationController::Handle(
    UpdateSimulationDraft const& command) {
  if (simulation_.State() != simulation::SimulationState::kEditing) {
    SetError("Restore the terrain before changing simulation parameters");
    return;
  }
  auto const valid = domain::ValidateLatticeSettings(command.draft.lattice);
  if (!valid) {
    SetError(valid.error());
    return;
  }
  if (scene_state_.dem &&
      command.draft.lattice != view_state_.simulation.lattice) {
    brush_.Finish();
    if (simulation_.Editing().changed) {
      pending_draft_ = command.draft;
      view_state_.confirm_discard = true;
      return;
    }
    auto initialized =
        simulation_.InitializeTerrain(scene_state_.dem, command.draft.lattice);
    if (!initialized) {
      SetError(initialized.error());
      return;
    }
    brush_.Reset(command.draft.lattice.height_subdivisions);
    scene_state_.lattice = simulation_.Update(0);
    ++scene_state_.revision;
  }
  view_state_.simulation = command.draft;
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(ImportTerrain const& command) {
  brush_.Finish();
  auto terrain = dem_loader_.Load(command.path);
  if (!terrain) {
    SetError(terrain.error().message);
    return;
  }

  auto initialized =
      simulation_.InitializeTerrain(*terrain, view_state_.simulation.lattice);
  if (!initialized) {
    SetError(initialized.error());
    return;
  }
  scene_state_.dem = std::move(*terrain);
  scene_state_.lattice = simulation_.Update(0);
  ++scene_state_.revision;
  view_state_.terrain_path = command.path;
  view_state_.terrain_loaded = true;
  view_state_.simulation_running = false;
  ResetEditor();
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(
    SetTerrainTexture const& command) {
  if (command.path.empty()) {
    scene_state_.terrain_texture.reset();
    view_state_.terrain_texture_path.clear();
    ++scene_state_.revision;
    view_state_.last_error.reset();
    return;
  }

  auto image = image_loader_.Load(command.path);
  if (!image) {
    SetError(image.error().message);
    return;
  }
  scene_state_.terrain_texture = std::move(*image);
  view_state_.terrain_texture_path = command.path;
  ++scene_state_.revision;
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(LoadProject const& command) {
  brush_.Finish();
  std::string const path = EnsureProjectExtension(command.path);
  auto document = project_repository_.Load(path);
  if (!document) {
    SetError(document.error().message);
    return;
  }

  auto valid = domain::ValidateLatticeSettings(document->simulation.lattice);
  if (!valid) {
    SetError("Invalid project: " + valid.error());
    return;
  }
  auto terrain = dem_loader_.Load(document->terrain_path);
  if (!terrain) {
    SetError(terrain.error().message);
    return;
  }

  auto initialized = simulation_.InitializeTerrain(
      *terrain, document->simulation.lattice, document->edits);
  if (!initialized) {
    SetError(initialized.error());
    return;
  }
  view_state_.simulation = document->simulation;
  view_state_.terrain_path = document->terrain_path;
  view_state_.terrain_texture_path = document->terrain_texture_path;
  view_state_.project_path = path;
  view_state_.terrain_loaded = true;
  view_state_.simulation_running = false;
  ResetEditor();
  view_state_.last_error.reset();
  scene_state_.dem = std::move(*terrain);
  scene_state_.terrain_texture.reset();
  scene_state_.lattice = simulation_.Update(0);
  ++scene_state_.revision;
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(SaveProject const& command) {
  std::string const path = EnsureProjectExtension(command.path);
  if (path.empty()) {
    SetError("Project path is empty");
    return;
  }
  brush_.Finish();
  domain::ProjectDocument document{
      .simulation = view_state_.simulation,
      .edits = simulation_.ExportEdits(),
      .terrain_path = view_state_.terrain_path,
      .terrain_texture_path = view_state_.terrain_texture_path,
  };
  auto result = project_repository_.Save(path, document);
  if (!result) {
    SetError(result.error().message);
    return;
  }
  view_state_.project_path = path;
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(NewProject const&) {
  brush_.Finish();
  simulation_.Clear();
  view_state_ = {};
  scene_state_ = {.revision = scene_state_.revision + 1U};
  ResetEditor();
}

void application::ApplicationController::Handle(PlaySimulation const&) {
  brush_.Finish();
  auto result = simulation_.Play(view_state_.simulation.lbm);
  if (!result) {
    SetError(result.error());
    return;
  }
  view_state_.last_error.reset();
  RefreshSimulationConfig();
  RefreshEditor();
}

void application::ApplicationController::Handle(PauseSimulation const&) {
  simulation_.Pause();
  view_state_.last_error.reset();
  RefreshSimulationConfig();
  RefreshEditor();
}

void application::ApplicationController::Handle(ResetSimulation const&) {
  if (!scene_state_.dem) {
    SetError("Import a DEM first");
    return;
  }
  brush_.Finish();
  auto result = simulation_.ResetToTerrain();
  if (!result) {
    SetError(result.error());
    return;
  }
  scene_state_.lattice = simulation_.Update(0);
  ++scene_state_.revision;
  RefreshSimulationConfig();
  ResetEditor();
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(RemoveDam const&) {
  auto result = simulation_.RemoveDam();
  if (!result) {
    SetError(result.error());
    return;
  }
  scene_state_.lattice = simulation_.Update(0);
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(RequestQuit const&) {
  should_quit_ = true;
}

void application::ApplicationController::RefreshSimulationConfig() {
  view_state_.validation_errors =
      domain::ValidateSimulationSettings(view_state_.simulation);
  auto state = simulation_.State();
  view_state_.simulation_running =
      state == simulation::SimulationState::kRunning;
  view_state_.simulation_paused = state == simulation::SimulationState::kPaused;
  view_state_.can_edit = view_state_.terrain_loaded &&
                         state == simulation::SimulationState::kEditing;
  view_state_.can_play = view_state_.terrain_loaded &&
                         state != simulation::SimulationState::kRunning &&
                         view_state_.validation_errors.empty();
  view_state_.can_remove_dam = simulation_.CanRemoveDam();
  view_state_.physical_step_count = simulation_.PhysicalStepCount();
}

void application::ApplicationController::SetError(std::string message) {
  view_state_.last_error = std::move(message);
}

void application::ApplicationController::RefreshEditor() {
  auto state = simulation_.Editing();
  view_state_.editor = brush_.Preview();
  scene_state_.preview = brush_.Preview();
  view_state_.has_edits = state.changed;
  view_state_.can_undo = view_state_.can_edit && state.can_undo;
  view_state_.can_redo = view_state_.can_edit && state.can_redo;
  view_state_.min_elevation = state.min_elevation;
  if (state.definition) {
    view_state_.max_elevation = state.definition->height - 1;
    view_state_.meters_per_cell = state.definition->meters_per_cell;
    view_state_.terrain_elevation_cells =
        state.definition->terrain_elevation_cells;
  }
}
void application::ApplicationController::ResetEditor() {
  brush_.Reset(view_state_.simulation.lattice.height_subdivisions);
  pending_draft_.reset();
  view_state_.confirm_discard = false;
  RefreshEditor();
}
void application::ApplicationController::Handle(SetBrush const& c) {
  if (!view_state_.can_edit) return;
  brush_.Settings(c.settings);
  RefreshEditor();
}
void application::ApplicationController::Handle(BrushPointer const& c) {
  auto pointer = c;
  pointer.blocked |= pending_draft_.has_value() || !view_state_.can_edit;
  auto result = brush_.Pointer(pointer);
  if (!result) SetError(result.error());
  RefreshEditor();
}
void application::ApplicationController::Handle(RunEditorAction const& c) {
  if (!view_state_.can_edit) return;
  auto result = brush_.Action(c.action);
  if (!result) SetError(result.error());
  RefreshEditor();
}
void application::ApplicationController::Handle(ConfirmDiscard const& c) {
  if (!pending_draft_) return;
  if (!c.confirm) {
    pending_draft_.reset();
    view_state_.confirm_discard = false;
    return;
  }
  auto draft = *pending_draft_;
  auto result = simulation_.InitializeTerrain(scene_state_.dem, draft.lattice);
  if (!result) {
    SetError(result.error());
    return;
  }
  view_state_.simulation = draft;
  ResetEditor();
  scene_state_.lattice = simulation_.Update(0);
  ++scene_state_.revision;
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}
