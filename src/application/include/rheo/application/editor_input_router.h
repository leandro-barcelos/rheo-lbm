#ifndef RHEO_APPLICATION_EDITOR_INPUT_ROUTER_H
#define RHEO_APPLICATION_EDITOR_INPUT_ROUTER_H
#include <functional>
#include <span>

#include "rheo/application/application_command.h"
#include "rheo/events/input_event.h"
namespace application {
// Input ownership is independent of ImGui/GLFW so releases and focus changes
// are processed even when the UI captures the pointer.
class EditorInputRouter {
 public:
  struct Capture {
    bool mouse = false, keyboard = false;
  };
  struct Extent {
    int width, height;
  };
  struct Callbacks {
    std::function<domain::Ray(double, double)> ray;
    std::function<void(events::InputEvent const&)> camera;
    std::function<void()> resize;
  };
  void Route(std::span<events::InputEvent const> events, Capture capture,
             Extent logical, Extent pixels, ICommandSink& commands,
             Callbacks const& callbacks);

 private:
  bool alt_pressed_ = false, left_ = false, right_ = false, middle_ = false,
       space_ = false, control_ = false, focused_ = true, navigating_ = false;
  glm::dvec2 cursor_{};
};
}  // namespace application
#endif
