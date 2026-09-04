#include "rheo/application/application_controller.h"

#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void Check(bool condition) {
  if (!condition) {
    throw std::runtime_error("test check failed");
  }
}

domain::SimulationSettingsDraft CompleteDraft() {
  return {
      .total_fluid_volume = 1000.0F,
      .initial_particle_spacing = 0.1F,
      .dem_resolution = 10.0F,
      .voxel_max_particles = 16,
      .viscosity = 5.0F,
      .rest_density = 1000.0F,
      .gas_constant = 10.0F,
      .coefficient_of_restitution = 0.5F,
      .friction = 0.01F,
      .yield_stress = 2.0F,
  };
}

domain::SharedTerrain Terrain(float elevation) {
  return std::make_shared<const domain::TerrainData>(domain::TerrainData{
      .samples = {{.uv = {0.0F, 0.0F},
                   .elevation = elevation,
                   .position = {0.0F, elevation, 0.0F}}},
      .width = 1,
      .height = 1,
  });
}

class FakeTerrainLoader final : public assets::ITerrainLoader {
 public:
  std::expected<domain::SharedTerrain, assets::AssetError> Load(
      std::string const& path, float) const override {
    if (path == failing_path) {
      return std::unexpected(assets::AssetError{"terrain failure"});
    }
    return terrain;
  }

  domain::SharedTerrain terrain = Terrain(1.0F);
  std::string failing_path;
};

class FakeImageLoader final : public assets::IImageLoader {
 public:
  std::expected<domain::SharedImage, assets::AssetError> Load(
      std::string const& path) const override {
    if (path == failing_path) {
      return std::unexpected(assets::AssetError{"image failure"});
    }
    return std::make_shared<const domain::ImageData>(
        domain::ImageData{.width = 1, .height = 1, .rgba = {1, 2, 3, 4}});
  }

  std::string failing_path;
};

class FakeProjectRepository final : public assets::IProjectRepository {
 public:
  std::expected<domain::ProjectDocument, assets::AssetError> Load(
      std::string const& path) const override {
    loaded_path = path;
    if (load_error) {
      return std::unexpected(assets::AssetError{"project failure"});
    }
    return document;
  }

  std::expected<void, assets::AssetError> Save(
      std::string const& path,
      domain::ProjectDocument const& value) const override {
    saved_path = path;
    saved_document = value;
    return {};
  }

  domain::ProjectDocument document;
  bool load_error = false;
  mutable std::string saved_path;
  mutable std::string loaded_path;
  mutable std::optional<domain::ProjectDocument> saved_document;
};

class FakeSimulation final : public simulation::ISimulationSession {
 public:
  void ApplyConfig(domain::SimulationConfig config) override {
    last_config = std::move(config);
    ++apply_count;
  }
  void Play() override {
    running = last_config.has_value();
    ++play_count;
  }
  void Pause() override {
    running = false;
    ++pause_count;
  }
  void Reset() override {
    running = false;
    ++reset_count;
  }
  void Clear() override {
    running = false;
    last_config.reset();
    ++clear_count;
  }
  std::optional<simulation::FluidRenderSnapshot> Update(double) override {
    return snapshot;
  }
  bool IsRunning() const override { return running; }
  bool IsReady() const override { return last_config.has_value(); }

  std::optional<domain::SimulationConfig> last_config;
  std::optional<simulation::FluidRenderSnapshot> snapshot;
  int apply_count = 0;
  int play_count = 0;
  int pause_count = 0;
  int reset_count = 0;
  int clear_count = 0;
  bool running = false;
};

}  // namespace

int main() {
  FakeTerrainLoader terrain_loader;
  FakeImageLoader image_loader;
  FakeProjectRepository repository;
  FakeSimulation simulation;
  application::ApplicationController controller(terrain_loader, image_loader,
                                                repository, simulation);

  controller.Submit(application::PlaySimulation{});
  controller.ProcessPendingCommands();
  Check(simulation.play_count == 0);
  Check(controller.ViewState().last_error.has_value());

  auto invalid_draft = CompleteDraft();
  invalid_draft.coefficient_of_restitution = 0.0F;
  controller.Submit(application::UpdateSimulationDraft{invalid_draft});
  controller.Submit(application::ImportTerrain{"first.tif"});
  controller.ProcessPendingCommands();
  Check(!controller.ViewState().can_play);
  Check(!controller.ViewState().validation_errors.empty());

  controller.Submit(application::UpdateSimulationDraft{CompleteDraft()});
  controller.ProcessPendingCommands();
  Check(controller.ViewState().can_play);
  Check(controller.ViewState().terrain_path == "first.tif");
  Check(controller.SceneState().terrain == terrain_loader.terrain);
  Check(simulation.apply_count == 1);

  controller.Submit(application::SetTerrainTexture{"first.png"});
  controller.ProcessPendingCommands();
  auto const first_texture = controller.SceneState().terrain_texture;
  Check(first_texture != nullptr);
  image_loader.failing_path = "broken.png";
  controller.Submit(application::SetTerrainTexture{"broken.png"});
  controller.ProcessPendingCommands();
  Check(controller.SceneState().terrain_texture == first_texture);
  Check(controller.ViewState().terrain_texture_path == "first.png");

  controller.Submit(application::PlaySimulation{});
  controller.Submit(application::PauseSimulation{});
  controller.Submit(application::ResetSimulation{});
  controller.ProcessPendingCommands();
  Check(simulation.play_count == 1);
  Check(simulation.pause_count == 1);
  Check(simulation.reset_count == 1);

  repository.document = {
      .simulation = CompleteDraft(),
      .terrain_path = "broken.tif",
      .terrain_texture_path = "new.png",
  };
  terrain_loader.failing_path = "broken.tif";
  auto const previous_terrain = controller.SceneState().terrain;
  auto const previous_revision = controller.SceneState().revision;
  controller.Submit(application::LoadProject{"broken.yaml"});
  controller.ProcessPendingCommands();
  Check(controller.ViewState().terrain_path == "first.tif");
  Check(controller.SceneState().terrain == previous_terrain);
  Check(controller.SceneState().revision == previous_revision);
  Check(controller.ViewState().last_error == "terrain failure");

  controller.Submit(application::SaveProject{"saved-project"});
  controller.ProcessPendingCommands();
  Check(repository.saved_path == "saved-project.yaml");
  Check(repository.saved_document.has_value());
  Check(repository.saved_document->terrain_path == "first.tif");

  terrain_loader.failing_path.clear();
  image_loader.failing_path.clear();
  repository.document = {
      .simulation = CompleteDraft(),
      .terrain_path = "loaded.tif",
      .terrain_texture_path = "loaded.png",
  };
  auto const revision_before_load = controller.SceneState().revision;
  controller.Submit(application::LoadProject{"loaded"});
  controller.ProcessPendingCommands();
  Check(repository.loaded_path == "loaded.yaml");
  Check(controller.ViewState().project_path == "loaded.yaml");
  Check(controller.ViewState().terrain_path == "loaded.tif");
  Check(controller.ViewState().terrain_texture_path == "loaded.png");
  Check(controller.ViewState().can_play);
  Check(!controller.ViewState().last_error.has_value());
  Check(controller.SceneState().revision == revision_before_load + 1);

  auto const populated_revision = controller.SceneState().revision;
  controller.Submit(application::NewProject{});
  controller.ProcessPendingCommands();
  Check(controller.SceneState().terrain == nullptr);
  Check(controller.SceneState().terrain_texture == nullptr);
  Check(controller.SceneState().revision == populated_revision + 1);

  controller.Submit(application::RequestQuit{});
  controller.ProcessPendingCommands();
  Check(controller.ShouldQuit());
}
