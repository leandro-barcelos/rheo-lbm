#include "rheo/domain/shallow_water.h"

#include <cmath>
#include <numbers>
#include <stdexcept>
namespace domain {
ShallowWaterSettings ShallowWaterParameters(ShallowWaterScenario scenario) {
  ShallowWaterSettings p;
  p.left = Boundary::Discharge;
  p.right = Boundary::Depth;
  p.sides = Boundary::Slip;
  switch (scenario) {
    case ShallowWaterScenario::Bump:
      p.nx = 500;
      p.ny = 50;
      p.dx = .05;
      p.dt = .05 / 15;
      p.tau = 1.5;
      p.max_steps = 20000;
      p.discharge = 4.42;
      p.outlet_depth = 2;
      break;
    case ShallowWaterScenario::Tide:
      p.nx = 800;
      p.ny = 3;
      p.dx = 17.5;
      p.dt = .0875;
      p.tau = .6;
      p.max_steps = 104200;
      p.left = Boundary::Tide;
      p.right = Boundary::Velocity;
      p.outlet_depth = 60.5;
      break;
    case ShallowWaterScenario::Cylinder:
      p.nx = 600;
      p.ny = 300;
      p.dx = 4. / 600;
      p.dt = .00145;
      p.tau = 1.982;
      p.max_steps = 40000;
      p.discharge = .248 / 2;
      p.outlet_depth = .185;
      p.manning = .012;
      break;
    case ShallowWaterScenario::Expansion:
      p.nx = 120;
      p.ny = 60;
      p.dx = .05;
      p.dt = .025;
      p.tau = 1;
      p.max_steps = 10000;
      p.discharge = .032;
      p.outlet_depth = .16;
      p.sides = Boundary::NoSlip;
      break;
    default:
      throw std::invalid_argument("Unknown shallow-water scenario");
  }
  return p;
}
ShallowWaterSnapshot ShallowWaterInitialState(ShallowWaterScenario scenario) {
  auto p = ShallowWaterParameters(scenario);
  ShallowWaterSnapshot s;
  s.nx = p.nx;
  s.ny = p.ny;
  auto n = std::size_t(p.nx) * p.ny;
  s.depth.resize(n);
  s.bed.resize(n);
  s.obstacles.resize(n);
  s.velocity_x.assign(n, 0);
  s.velocity_y.assign(n, 0);
  for (int y = 0; y < p.ny; ++y)
    for (int x = 0; x < p.nx; ++x) {
      int i = x + y * p.nx;
      double px = x * p.dx, py = (y + .5) * p.dx;
      switch (scenario) {
        case ShallowWaterScenario::Bump:
          s.bed[i] = px > 8 && px < 12 ? .2 - .05 * (px - 10) * (px - 10) : 0;
          break;
        case ShallowWaterScenario::Tide:
          s.bed[i] =
              60.5 - (50.5 - 40 * px / 14000 -
                      10 * std::sin(std::numbers::pi * (4 * px / 14000 - .5)));
          break;
        case ShallowWaterScenario::Cylinder:
          s.bed[i] = -6.25e-4 * px;
          s.obstacles[i] = std::hypot((x + .5) * p.dx - 2, py - 1) <= .11;
          break;
        case ShallowWaterScenario::Expansion:
          s.obstacles[i] = x < 40 && (y < 20 || y >= 40);
          break;
      }
      double surface = p.outlet_depth;
      if (scenario == ShallowWaterScenario::Cylinder)
        surface += -6.25e-4 * (p.nx - 1) * p.dx;
      s.depth[i] = surface - s.bed[i];
    }
  return s;
}
}  // namespace domain
