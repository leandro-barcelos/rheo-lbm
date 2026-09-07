#ifndef RHEO_APPLICATION_LATTICE_BRUSH_CONTROLLER_H
#define RHEO_APPLICATION_LATTICE_BRUSH_CONTROLLER_H
#include "rheo/simulation/simulation_session.h"
namespace application {
struct BrushPointer {
  domain::Ray ray;
  glm::dvec2 cursor{};
  bool pressed = false, released = false, blocked = false;
};
enum class EditorAction { kUndo, kRedo, kFinishDam, kRemovePoint, kCancelDam };
class LatticeBrushController {
 public:
  explicit LatticeBrushController(simulation::ISimulationSession& session)
      : session_(session) {}
  void Reset(int subdivisions);
  void Finish();
  void Settings(domain::BrushSettings settings);
  std::expected<void, std::string> Pointer(BrushPointer const& pointer);
  std::expected<void, std::string> Action(EditorAction action);
  domain::BrushPreview const& Preview() const { return preview_; }

 private:
  simulation::ISimulationSession& session_;
  domain::BrushPreview preview_;
  bool painting_ = false;
  glm::dvec2 last_cursor_{};
};
}  // namespace application
#endif
