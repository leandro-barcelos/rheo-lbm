#include "rheo/application/application_controller.h"

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
      simulation_(simulation) {}

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
  scene_state_.lattice = simulation_.Update(delta_ms);
  view_state_.simulation_running = simulation_.IsRunning();
}

void application::ApplicationController::Handle(
    UpdateSimulationDraft const& command) {
  auto const valid = domain::ValidateLatticeSettings(command.draft.lattice);
  if (!valid) {
    SetError(valid.error());
    return;
  }
  if (scene_state_.dem &&
      command.draft.lattice != view_state_.simulation.lattice) {
    auto initialized =
        simulation_.InitializeTerrain(scene_state_.dem, command.draft.lattice);
    if (!initialized) {
      SetError(initialized.error());
      return;
    }
    scene_state_.lattice = simulation_.Update(0);
    ++scene_state_.revision;
  }
  view_state_.simulation = command.draft;
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(ImportTerrain const& command) {
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

  auto initialized =
      simulation_.InitializeTerrain(*terrain, document->simulation.lattice);
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
  domain::ProjectDocument document{
      .simulation = view_state_.simulation,
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
  simulation_.Clear();
  view_state_ = {};
  scene_state_ = {.revision = scene_state_.revision + 1U};
}

void application::ApplicationController::Handle(PlaySimulation const&) {
  SetError("Fluid dynamics is not available yet");
}

void application::ApplicationController::Handle(PauseSimulation const&) {
  simulation_.Pause();
  view_state_.simulation_running = false;
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(ResetSimulation const&) {
  if (!scene_state_.dem) {
    SetError("Import a DEM first");
    return;
  }
  auto result = simulation_.InitializeTerrain(scene_state_.dem,
                                              view_state_.simulation.lattice);
  if (!result) {
    SetError(result.error());
    return;
  }
  scene_state_.lattice = simulation_.Update(0);
  ++scene_state_.revision;
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(RequestQuit const&) {
  should_quit_ = true;
}

void application::ApplicationController::RefreshSimulationConfig() {
  view_state_.validation_errors.clear();
  if (auto valid =
          domain::ValidateLatticeSettings(view_state_.simulation.lattice);
      !valid)
    view_state_.validation_errors.push_back(valid.error());
  view_state_.can_play = false;
  view_state_.simulation_running = false;
}

void application::ApplicationController::SetError(std::string message) {
  view_state_.last_error = std::move(message);
}
