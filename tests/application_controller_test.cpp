#include "rheo/application/application_controller.h"

#include <stdexcept>
#include <string>
using namespace application;
void Check(bool value) {
  if (!value) throw std::runtime_error("controller check failed");
}
class FakeDemLoader final : public assets::IDemLoader {
 public:
  domain::SharedDem dem = std::make_shared<domain::DemData>(
      domain::DemData{.samples = {{.coordinate = {0, 0}, .elevation = 1}},
                      .width = 1,
                      .height = 1,
                      .pixel_size_meters = {10, 10},
                      .min_elevation = 1,
                      .max_elevation = 1});
  std::expected<domain::SharedDem, assets::AssetError> Load(
      std::string const& path) const override {
    if (path == "bad")
      return std::unexpected(assets::AssetError{"read failed"});
    return dem;
  }
};
class FakeImageLoader final : public assets::IImageLoader {
 public:
  std::expected<domain::SharedImage, assets::AssetError> Load(
      std::string const&) const override {
    return std::make_shared<const domain::ImageData>(
        domain::ImageData{.width = 1, .height = 1, .rgba = {1, 2, 3, 4}});
  }
};
class FakeRepository final : public assets::IProjectRepository {
 public:
  mutable domain::ProjectDocument document;
  std::expected<domain::ProjectDocument, assets::AssetError> Load(
      std::string const&) const override {
    return document;
  }
  std::expected<void, assets::AssetError> Save(
      std::string const&, domain::ProjectDocument const& value) const override {
    document = value;
    return {};
  }
};
class FakeSession final : public simulation::ISimulationSession {
 public:
  int initializations = 0, clears = 0;
  bool fail = false;
  std::optional<simulation::LatticeRenderSnapshot> snapshot;
  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem, domain::LatticeSettings) override {
    ++initializations;
    if (fail) return std::unexpected("GPU failure");
    snapshot = simulation::LatticeRenderSnapshot{
        .cell_count = 13,
        .ready_signal = static_cast<std::uint64_t>(initializations)};
    return {};
  }
  void Play() override { throw std::runtime_error("SPH must remain disabled"); }
  void Pause() override {}
  void Clear() override {
    ++clears;
    snapshot.reset();
  }
  std::optional<simulation::LatticeRenderSnapshot> Update(double) override {
    return snapshot;
  }
  bool IsRunning() const override { return false; }
  bool IsReady() const override { return snapshot.has_value(); }
};
int main() {
  FakeDemLoader loader;
  FakeImageLoader images;
  FakeRepository repository;
  FakeSession session;
  ApplicationController app(loader, images, repository, session);
  auto submit = [&](ApplicationCommand command) {
    app.Submit(std::move(command));
    app.ProcessPendingCommands();
  };
  submit(ImportTerrain{"dem.tif"});
  Check(app.ViewState().terrain_loaded && !app.ViewState().can_play);
  Check(app.SceneState().dem == loader.dem &&
        app.SceneState().lattice.has_value());
  Check(session.initializations == 1 && !app.ViewState().last_error);
  for (int i = 0; i < 10; ++i) app.Update(16);
  Check(session.initializations == 1);
  auto draft = app.ViewState().simulation;
  draft.viscosity = 5;
  submit(UpdateSimulationDraft{draft});
  Check(session.initializations == 1 && app.SceneState().lattice.has_value());
  draft.lattice.height_subdivisions = 26;
  submit(UpdateSimulationDraft{draft});
  Check(session.initializations == 2 &&
        app.ViewState().simulation.lattice.height_subdivisions == 26);
  auto revision = app.SceneState().revision;
  draft.lattice.height_subdivisions = 0;
  submit(UpdateSimulationDraft{draft});
  Check(session.initializations == 2 && app.SceneState().revision == revision &&
        app.ViewState().last_error.has_value());
  session.fail = true;
  draft.lattice.height_subdivisions = 30;
  submit(UpdateSimulationDraft{draft});
  Check(app.ViewState().simulation.lattice.height_subdivisions == 26 &&
        app.SceneState().revision == revision);
  submit(ImportTerrain{"other.tif"});
  Check(app.ViewState().terrain_path == "dem.tif" &&
        app.SceneState().revision == revision);
  submit(ImportTerrain{"bad"});
  Check(app.SceneState().dem == loader.dem);
  repository.document.terrain_path = "project.tif";
  submit(LoadProject{"project"});
  Check(app.ViewState().terrain_path == "dem.tif");
  session.fail = false;
  submit(LoadProject{"project"});
  Check(app.ViewState().terrain_path == "project.tif" &&
        app.SceneState().lattice.has_value());
  Check(!app.ViewState().last_error &&
        !app.ViewState().simulation.total_fluid_volume);
  int before = session.initializations;
  submit(ResetSimulation{});
  Check(session.initializations == before + 1 &&
        app.SceneState().lattice.has_value());
  submit(PlaySimulation{});
  Check(!app.ViewState().simulation_running);
  submit(SaveProject{"saved"});
  Check(repository.document.terrain_path == "project.tif");
  submit(NewProject{});
  Check(!app.SceneState().dem && !app.SceneState().lattice &&
        session.clears == 1);
  Check(app.ViewState().simulation.lattice.height_subdivisions == 13);
}
