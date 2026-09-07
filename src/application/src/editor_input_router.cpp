#include "rheo/application/editor_input_router.h"

#include <algorithm>
namespace application {
void EditorInputRouter::Route(std::span<events::InputEvent const> events,
                              Capture capture, Extent logical, Extent pixels,
                              ICommandSink& commands,
                              Callbacks const& callbacks) {
  auto framebuffer = [&] {
    return cursor_ *
           glm::dvec2(double(pixels.width) / std::max(logical.width, 1),
                      double(pixels.height) / std::max(logical.height, 1));
  };
  auto pointer = [&](bool pressed = false, bool released = false,
                     bool blocked = false) {
    auto p = framebuffer();
    commands.Submit(application::BrushPointer{
        callbacks.ray(p.x, p.y), cursor_, pressed, released,
        blocked || capture.mouse || !focused_ || navigating_});
  };
  auto navigation = [&] {
    bool active =
        focused_ && !capture.mouse && (right_ || middle_ || (space_ && left_));
    if (active != navigating_) {
      navigating_ = active;
      pointer(false, true, active);
      if (active) {
        callbacks.camera(events::MouseButtonPressedEvent{events::kButtonLeft});
        auto p = framebuffer();
        callbacks.camera(events::MouseMovedEvent{float(p.x), float(p.y)});
      } else
        callbacks.camera(events::MouseButtonReleasedEvent{events::kButtonLeft});
    }
  };
  for (auto const& event : events) {
    if (auto e = std::get_if<events::WindowResizedEvent>(&event)) {
      callbacks.resize();
      callbacks.camera(event);

      continue;
    }
    if (auto e = std::get_if<events::WindowFocusEvent>(&event)) {
      focused_ = e->focused;
      if (!focused_) {
        left_ = right_ = middle_ = space_ = control_ = alt_pressed_ = false;
        pointer(false, true, true);
      }
      navigation();
      continue;
    }
    if (auto e = std::get_if<events::KeyPressedEvent>(&event)) {
      if (e->key == events::kLeftAlt || e->key == events::kRightAlt)
        alt_pressed_ = true;
      if (e->key == events::kF4 && alt_pressed_)
        commands.Submit(application::RequestQuit{});
      if (!capture.keyboard) {
        if (e->key == events::kSpace) space_ = true;
        if (e->key == events::kLeftControl || e->key == events::kRightControl)
          control_ = true;
        if (!e->repeated) {
          if (e->key == events::kEscape)
            commands.Submit(application::RunEditorAction{
                application::EditorAction::kCancelDam});
          if (control_ && e->key == events::kZ)
            commands.Submit(
                application::RunEditorAction{application::EditorAction::kUndo});
          if (control_ && e->key == events::kY)
            commands.Submit(
                application::RunEditorAction{application::EditorAction::kRedo});
        }
      }
      navigation();
      continue;
    }
    if (auto e = std::get_if<events::KeyReleasedEvent>(&event)) {
      if (e->key == events::kSpace) space_ = false;
      if (e->key == events::kLeftControl || e->key == events::kRightControl)
        control_ = false;
      if (e->key == events::kLeftAlt || e->key == events::kRightAlt)
        alt_pressed_ = false;
      navigation();
      continue;
    }
    if (auto e = std::get_if<events::MouseButtonPressedEvent>(&event)) {
      if (!capture.mouse) {
        if (e->button == events::kButtonLeft) left_ = true;
        if (e->button == events::kButtonRight) right_ = true;
        if (e->button == events::kButtonMiddle) middle_ = true;
      }
      navigation();
      if (e->button == events::kButtonLeft) pointer(true);
      continue;
    }
    if (auto e = std::get_if<events::MouseButtonReleasedEvent>(&event)) {
      if (e->button == events::kButtonLeft) {
        left_ = false;
        pointer(false, true);
      }
      if (e->button == events::kButtonRight) right_ = false;
      if (e->button == events::kButtonMiddle) middle_ = false;
      navigation();
      continue;
    }
    if (auto e = std::get_if<events::MouseMovedEvent>(&event)) {
      cursor_ = {e->x, e->y};
      navigation();
      auto p = framebuffer();
      if (navigating_)
        callbacks.camera(events::MouseMovedEvent{float(p.x), float(p.y)});
      pointer();
      continue;
    }
    if (std::holds_alternative<events::MouseScrolledEvent>(event) &&
        !capture.mouse) {
      pointer(false, true, true);
      callbacks.camera(event);
    }
  }
  navigation();
  pointer();
}
}  // namespace application
