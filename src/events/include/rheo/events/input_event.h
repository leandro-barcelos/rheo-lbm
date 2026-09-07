#ifndef RHEO_EVENTS_INPUT_EVENT_H
#define RHEO_EVENTS_INPUT_EVENT_H

#include <variant>

#include "rheo/events/key_codes.h"
#include "rheo/events/mouse_codes.h"

namespace events {

struct KeyPressedEvent {
  KeyCode key;
  bool repeated = false;
};

struct KeyReleasedEvent {
  KeyCode key;
};

struct MouseMovedEvent {
  float x = 0.0F;
  float y = 0.0F;
};

struct MouseScrolledEvent {
  float y_offset = 0.0F;
};

struct MouseButtonPressedEvent {
  MouseCode button;
  bool repeated = false;
};

struct MouseButtonReleasedEvent {
  MouseCode button;
};

struct WindowResizedEvent {
  int width = 0;
  int height = 0;
};

struct WindowFocusEvent {
  bool focused = true;
};

using InputEvent =
    std::variant<KeyPressedEvent, KeyReleasedEvent, MouseMovedEvent,
                 MouseScrolledEvent, MouseButtonPressedEvent,
                 MouseButtonReleasedEvent, WindowResizedEvent,
                 WindowFocusEvent>;

[[nodiscard]] inline bool IsKeyboardEvent(InputEvent const& event) {
  return std::holds_alternative<KeyPressedEvent>(event) ||
         std::holds_alternative<KeyReleasedEvent>(event);
}

[[nodiscard]] inline bool IsMouseEvent(InputEvent const& event) {
  return std::holds_alternative<MouseMovedEvent>(event) ||
         std::holds_alternative<MouseScrolledEvent>(event) ||
         std::holds_alternative<MouseButtonPressedEvent>(event) ||
         std::holds_alternative<MouseButtonReleasedEvent>(event);
}

}  // namespace events

#endif  // RHEO_EVENTS_INPUT_EVENT_H
