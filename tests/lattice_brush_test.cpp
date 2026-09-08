#include <source_location>
#include <stdexcept>

#include "rheo/application/lattice_brush_controller.h"
using namespace application;
void Check(bool v, std::source_location l = std::source_location::current()) {
  if (!v)
    throw std::runtime_error("Brush failed at " + std::to_string(l.line()));
}
class Session : public simulation::ISimulationSession {
 public:
  domain::LatticeDefinition d{9, 6, 9, 486, 1, 5};
  domain::CellTypes types =
      domain::CellTypes(486, domain::Type(domain::CellType::kGas));
  domain::EditHistory history;
  int applications = 0;
  Session() { domain::RebuildInterfaces(types, d); }
  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem, domain::LatticeSettings,
      std::optional<domain::LatticeEdits> const&) override {
    return {};
  }
  simulation::EditState Editing() const override {
    return {d, types, 0, false, history.CanUndo(), history.CanRedo()};
  }
  void BeginStroke() override { history.Begin(); }
  void EndStroke() override { history.End(types); }
  std::expected<void, std::string> Edit(
      simulation::EditOperation const& op) override {
    ++applications;
    auto next = types;
    domain::ApplyBrush(next, d, op.center, op.brush);
    history.Capture(domain::Differences(types, next));
    types = std::move(next);
    return {};
  }
  std::optional<domain::LatticeEdits> ExportEdits() const override {
    return {};
  }
  void Clear() override {}
  std::expected<void, std::string> Play(domain::LbmSettings const&) override {
    return {};
  }
  void Pause() override {}
  std::expected<void, std::string> ResetToTerrain() override { return {}; }
  std::expected<void, std::string> RemoveDam() override { return {}; }
  bool IsRunning() const override { return false; }
  simulation::SimulationState State() const override {
    return simulation::SimulationState::kEditing;
  }
  std::uint64_t PhysicalStepCount() const override { return 0; }
  bool CanRemoveDam() const override { return false; }
  bool IsReady() const override { return true; }
  std::optional<simulation::LatticeRenderSnapshot> Update(double) override {
    return {};
  }
};
int main() {
  Session s;
  LatticeBrushController brush(s);
  brush.Reset(6);
  Check(brush.Preview().settings.elevation == 3 &&
        brush.Preview().settings.radius == 2);
  brush.Settings(
      {.mode = domain::BrushMode::kElevation, .radius = 1, .elevation = 2});
  BrushPointer pointer{{{0, 2, 0}, {0, -1, 0}}, {100, 100}, true};
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 1 && s.history.Recording());
  pointer.pressed = false;
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 1);
  pointer.cursor.x += 1;
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 2);
  pointer.released = true;
  pointer.blocked = true;
  Check(brush.Pointer(pointer).has_value());
  Check(!s.history.Recording() && s.history.CanUndo());
  pointer.released = false;
  pointer.blocked = false;
  pointer.cursor.x += 2;
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 2);
  pointer.pressed = true;
  Check(brush.Pointer(pointer).has_value());
  brush.Settings({.mode = domain::BrushMode::kTerrain});
  Check(!s.history.Recording());
  pointer.pressed = false;
  pointer.cursor.x += 1;
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 3);
  pointer.pressed = true;
  pointer.blocked = true;
  Check(brush.Pointer(pointer).has_value());
  Check(s.applications == 3 && !brush.Preview().active);
  pointer.blocked = false;
  brush.Settings({.mode = domain::BrushMode::kSampleElevation});
  Check(brush.Pointer(pointer).has_value());
  Check(brush.Preview().settings.mode == domain::BrushMode::kElevation &&
        brush.Preview().settings.elevation == 2);
  brush.Settings({.mode = domain::BrushMode::kDam});
  Check(brush.Pointer(pointer).has_value());
  Check(brush.Preview().dam_points.size() == 1);
  Check(brush.Preview().dam_points.front().y == 2);
  pointer.pressed = false;
  pointer.cursor.x += 3;
  Check(brush.Pointer(pointer).has_value());
  Check(brush.Preview().dam_points.size() == 1);
  Check(brush.Action(EditorAction::kCancelDam).has_value());
  Check(brush.Preview().dam_points.empty());
  pointer.pressed = true;
  Check(brush.Pointer(pointer).has_value());
  brush.Reset(6);
  Check(brush.Preview().dam_points.empty() && !brush.Preview().active);
}
