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
    assets::ITerrainLoader const& terrain_loader,
    assets::IImageLoader const& image_loader,
    assets::IProjectRepository const& project_repository,
    simulation::ISimulationSession& simulation)
    : terrain_loader_(terrain_loader),
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
  scene_state_.fluid = simulation_.Update(delta_ms);
  view_state_.simulation_running = simulation_.IsRunning();
}

void application::ApplicationController::Handle(
    UpdateSimulationDraft const& command) {
  view_state_.simulation = command.draft;
  view_state_.last_error.reset();
  RefreshSimulationConfig();
}

void application::ApplicationController::Handle(ImportTerrain const& command) {
  float const resolution =
      view_state_.simulation.dem_resolution.value_or(10.0F);
  if (resolution <= 0.0F) {
    SetError("DEM resolution must be greater than zero");
    return;
  }
  auto terrain = terrain_loader_.Load(command.path, resolution);
  if (!terrain) {
    SetError(terrain.error().message);
    return;
  }

  simulation_.Clear();
  scene_state_.terrain = std::move(*terrain);
  scene_state_.fluid.reset();
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

  auto const validation_errors =
      domain::ValidateSimulationSettings(document->simulation);
  if (!validation_errors.empty()) {
    SetError("Invalid project: " + validation_errors.front());
    return;
  }

  float const resolution = document->simulation.dem_resolution.value_or(10.0F);
  auto terrain = terrain_loader_.Load(document->terrain_path, resolution);
  if (!terrain) {
    SetError(terrain.error().message);
    return;
  }

  domain::SharedImage texture;
  if (!document->terrain_texture_path.empty()) {
    auto image = image_loader_.Load(document->terrain_texture_path);
    if (!image) {
      SetError(image.error().message);
      return;
    }
    texture = std::move(*image);
  }

  simulation_.Clear();
  view_state_.simulation = document->simulation;
  view_state_.terrain_path = document->terrain_path;
  view_state_.terrain_texture_path = document->terrain_texture_path;
  view_state_.project_path = path;
  view_state_.terrain_loaded = true;
  view_state_.simulation_running = false;
  view_state_.last_error.reset();
  scene_state_.terrain = std::move(*terrain);
  scene_state_.terrain_texture = std::move(texture);
  scene_state_.fluid.reset();
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
  if (!view_state_.can_play) {
    SetError("Define all simulation parameters and import a terrain first");
    return;
  }
  simulation_.Play();
  view_state_.simulation_running = simulation_.IsRunning();
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(PauseSimulation const&) {
  simulation_.Pause();
  view_state_.simulation_running = false;
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(ResetSimulation const&) {
  if (!view_state_.can_play) {
    SetError("Cannot reset an incomplete simulation");
    return;
  }
  simulation_.Reset();
  view_state_.simulation_running = false;
  scene_state_.fluid = simulation_.Update(0.0);
  view_state_.last_error.reset();
}

void application::ApplicationController::Handle(RequestQuit const&) {
  should_quit_ = true;
}

void application::ApplicationController::RefreshSimulationConfig() {
  view_state_.validation_errors =
      domain::ValidateSimulationSettings(view_state_.simulation);
  if (scene_state_.terrain == nullptr || !scene_state_.terrain->IsValid()) {
    view_state_.validation_errors.emplace_back("Import a valid terrain");
  }

  auto config = domain::ValidateSimulationConfig(view_state_.simulation,
                                                 scene_state_.terrain);
  view_state_.can_play = config.has_value();
  if (!config) {
    if (simulation_.IsRunning() || simulation_.IsReady()) {
      simulation_.Clear();
    }
    view_state_.simulation_running = false;
    scene_state_.fluid.reset();
    return;
  }

  simulation_.ApplyConfig(std::move(*config));
  view_state_.simulation_running = false;
  scene_state_.fluid.reset();
}

void application::ApplicationController::SetError(std::string message) {
  view_state_.last_error = std::move(message);
}
