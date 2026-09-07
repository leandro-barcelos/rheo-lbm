#include <source_location>
#include <stdexcept>

#include "rheo/application/editor_input_router.h"
using namespace application;
void Check(bool v, std::source_location l = std::source_location::current()) {
  if (!v)
    throw std::runtime_error("Input router failed at " +
                             std::to_string(l.line()));
}
struct Sink : ICommandSink {
  std::vector<ApplicationCommand> commands;
  void Submit(ApplicationCommand c) override {
    commands.push_back(std::move(c));
  }
};
int main() {
  EditorInputRouter router;
  Sink sink;
  std::vector<events::InputEvent> camera;
  glm::dvec2 pixel{};
  int resizes = 0;
  EditorInputRouter::Callbacks callbacks{
      [&](double x, double y) {
        pixel = {x, y};
        return domain::Ray{{float(x), float(y), 0}, {0, -1, 0}};
      },
      [&](auto const& e) { camera.push_back(e); }, [&] { ++resizes; }};
  auto route = [&](std::initializer_list<events::InputEvent> events,
                   EditorInputRouter::Capture capture =
                       EditorInputRouter::Capture{}) {
    sink.commands.clear();
    camera.clear();
    router.Route(events, capture, {800, 600}, {1600, 1200}, sink, callbacks);
  };
  auto last = [&] { return std::get<BrushPointer>(sink.commands.back()); };
  route({events::MouseMovedEvent{100, 200}});
  Check(pixel == glm::dvec2(200, 400) && !last().blocked);
  route({events::MouseButtonPressedEvent{events::kButtonLeft}});
  Check(std::get<BrushPointer>(sink.commands.front()).pressed &&
        camera.empty());
  route({events::MouseButtonReleasedEvent{events::kButtonLeft}}, {true, false});
  Check(std::get<BrushPointer>(sink.commands.front()).released &&
        last().blocked);
  route({events::MouseButtonPressedEvent{events::kButtonRight}});
  Check(last().blocked && camera.size() == 2 &&
        std::holds_alternative<events::MouseButtonPressedEvent>(camera[0]));
  route({events::MouseMovedEvent{110, 210}});
  Check(last().blocked &&
        std::get<events::MouseMovedEvent>(camera[0]).x == 220);
  route({events::MouseButtonReleasedEvent{events::kButtonRight}},
        {true, false});
  Check(camera.size() == 1 &&
        std::holds_alternative<events::MouseButtonReleasedEvent>(camera[0]));
  route({events::MouseButtonPressedEvent{events::kButtonLeft},
         events::KeyPressedEvent{events::kSpace}});
  Check(last().blocked);
  route({events::WindowFocusEvent{false}});
  Check(last().blocked && camera.size() == 1);
  route({events::WindowFocusEvent{true}, events::MouseMovedEvent{120, 220}});
  Check(!last().blocked && camera.empty());
  route({events::KeyPressedEvent{events::kLeftControl},
         events::KeyPressedEvent{events::kZ}});
  Check(std::get<RunEditorAction>(sink.commands.front()).action ==
        EditorAction::kUndo);
  route({events::KeyPressedEvent{events::kY}});
  Check(std::get<RunEditorAction>(sink.commands.front()).action ==
        EditorAction::kRedo);
  route({events::KeyPressedEvent{events::kZ}}, {false, true});
  Check(sink.commands.size() == 1);
  route({events::KeyReleasedEvent{events::kLeftControl}}, {false, true});
  route({events::KeyPressedEvent{events::kEscape}});
  Check(std::get<RunEditorAction>(sink.commands.front()).action ==
        EditorAction::kCancelDam);
  route({events::MouseScrolledEvent{1}});
  Check(camera.size() == 1 &&
        std::get<BrushPointer>(sink.commands.front()).released);
  route({events::WindowResizedEvent{1600, 1200}});
  Check(resizes == 1);
}
