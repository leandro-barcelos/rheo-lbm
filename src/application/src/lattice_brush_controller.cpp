#include "rheo/application/lattice_brush_controller.h"

#include <algorithm>
namespace application {
void LatticeBrushController::Finish() {
  session_.EndStroke();
  painting_ = false;
}
void LatticeBrushController::Reset(int subdivisions) {
  Finish();
  preview_ = {};
  auto state = session_.Editing();
  preview_.settings.elevation = std::clamp(
      subdivisions / 2, 0,
      state.definition ? int(state.definition->height) - 1 : subdivisions);
}
void LatticeBrushController::Settings(domain::BrushSettings settings) {
  auto state = session_.Editing();
  settings.radius = std::clamp(settings.radius, 1, 32);
  settings.dam_half_width = std::clamp(settings.dam_half_width, 0, 16);
  settings.elevation =
      std::clamp(settings.elevation, 0,
                 state.definition ? int(state.definition->height) - 1 : 0);
  if (settings != preview_.settings) {
    Finish();
    preview_.active = false;
  }
  if (settings.mode != preview_.settings.mode) preview_.dam_points.clear();
  preview_.settings = settings;
}
std::expected<void, std::string> LatticeBrushController::Pointer(
    BrushPointer const& p) {
  if (p.released || p.blocked) Finish();
  preview_.active = false;
  auto state = session_.Editing();
  if (p.blocked || !state.definition) return {};
  auto hit = session_.Select(p.ray);
  if (!hit) return {};
  auto mode = preview_.settings.mode;
  auto center = hit->surface;
  if (mode == domain::BrushMode::kTerrain ||
      mode == domain::BrushMode::kWater) {
    center = hit->previous_gas;
  } else if (mode == domain::BrushMode::kElevation) {
    center.y = preview_.settings.elevation;
  } else if (mode == domain::BrushMode::kSampleElevation ||
             mode == domain::BrushMode::kDam) {
    center.y = domain::TerrainElevation(state.types, *state.definition,
                                        hit->surface.x, hit->surface.z);
  }
  if (!domain::Inside(center, *state.definition)) return {};
  preview_.active = true;
  preview_.center = center;
  if (p.pressed) {
    Finish();
    if (mode == domain::BrushMode::kSampleElevation) {
      auto settings = preview_.settings;
      settings.elevation = domain::TerrainElevation(
          state.types, *state.definition, center.x, center.z);
      settings.mode = domain::BrushMode::kElevation;
      Settings(settings);
      return {};
    }
    if (mode == domain::BrushMode::kDam) {
      if (preview_.dam_points.empty() || preview_.dam_points.back() != center)
        preview_.dam_points.push_back(center);
      return {};
    }
    session_.BeginStroke();
    painting_ = true;
  }
  if (!painting_ || (!p.pressed && glm::length(p.cursor - last_cursor_) < .5))
    return {};
  last_cursor_ = p.cursor;
  auto result = session_.Edit({.center = center, .brush = preview_.settings});
  if (mode == domain::BrushMode::kWater || !result) Finish();
  return result;
}
std::expected<void, std::string> LatticeBrushController::Action(
    EditorAction action) {
  Finish();
  switch (action) {
    case EditorAction::kUndo:
      return session_.Edit({.kind = simulation::EditOperation::Kind::kUndo});
    case EditorAction::kRedo:
      return session_.Edit({.kind = simulation::EditOperation::Kind::kRedo});
    case EditorAction::kRemovePoint:
      if (!preview_.dam_points.empty()) preview_.dam_points.pop_back();
      break;
    case EditorAction::kCancelDam:
      preview_.dam_points.clear();
      break;
    case EditorAction::kFinishDam: {
      session_.BeginStroke();
      auto result =
          session_.Edit({.kind = simulation::EditOperation::Kind::kDam,
                         .brush = preview_.settings,
                         .points = preview_.dam_points});
      Finish();
      if (result) preview_.dam_points.clear();
      return result;
    }
  }
  return {};
}
}  // namespace application
