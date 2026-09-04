#include "rheo/events/input_queue.h"

#include <stdexcept>
#include <variant>

namespace {

void Check(bool condition) {
  if (!condition) {
    throw std::runtime_error("test check failed");
  }
}

}  // namespace

int main() {
  events::InputQueue queue;
  queue.Push(events::KeyPressedEvent{events::kA, false});
  queue.Push(events::MouseMovedEvent{12.0F, 24.0F});
  queue.Push(events::WindowResizedEvent{800, 600});

  auto events = queue.Drain();
  Check(events.size() == 3);
  Check(std::holds_alternative<events::KeyPressedEvent>(events[0]));
  Check(std::holds_alternative<events::MouseMovedEvent>(events[1]));
  Check(std::holds_alternative<events::WindowResizedEvent>(events[2]));
  Check(std::get<events::KeyPressedEvent>(events[0]).key == events::kA);
  Check(std::get<events::MouseMovedEvent>(events[1]).x == 12.0F);
  Check(std::get<events::WindowResizedEvent>(events[2]).width == 800);
  Check(queue.Empty());
  Check(queue.Drain().empty());
}
