#include <source_location>
#include <stdexcept>

#include "camera.h"
void Check(bool v, std::source_location l = std::source_location::current()) {
  if (!v)
    throw std::runtime_error("Camera picking failed at " +
                             std::to_string(l.line()));
}
int main() {
  domain::LatticeDefinition d{10, 4, 10, 400, 1, 3};
  domain::CellTypes t(400, domain::Type(domain::CellType::kGas));
  domain::RebuildInterfaces(t, d);
  renderer::Camera camera({0, 2, 0}, {800, 600});
  camera.InitTopView({-.5, -.2, -.5}, {.5, .2, .5});
  for (auto size :
       {platform::WindowSize{800, 600}, platform::WindowSize{1600, 1200},
        platform::WindowSize{1200, 600}}) {
    camera.OnWindowResizedEvent({size.width, size.height});
    auto center = domain::PickCell(
        camera.ScreenRay(size.width * .5, size.height * .5), d, t);
    auto north = domain::PickCell(
        camera.ScreenRay(size.width * .5, size.height * .35), d, t);
    auto east = domain::PickCell(
        camera.ScreenRay(size.width * .6, size.height * .5), d, t);
    Check(center && north && east);
    Check(north->surface.z > center->surface.z &&
          east->surface.x > center->surface.x);
    auto logical_x = size.width * .5 / 2, logical_y = size.height * .35 / 2;
    auto scaled =
        domain::PickCell(camera.ScreenRay(logical_x * 2, logical_y * 2), d, t);
    Check(scaled && scaled->surface == north->surface);
  }
}
