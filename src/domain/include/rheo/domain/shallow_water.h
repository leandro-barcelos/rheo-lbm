#pragma once
#include <cstdint>
#include <vector>
namespace domain {
enum class Boundary {
  Periodic,
  Slip,
  NoSlip,
  ZeroGradient,
  Discharge,
  Velocity,
  Depth,
  Tide
};
struct ShallowWaterSettings {
  int nx = 32, ny = 16;
  // SI units: dx [m], dt [s], gravity [m/s^2]; tau is dimensionless.
  double dx = 1, dt = .01, gravity = 9.81, tau = 1, manning = 0;
  std::uint64_t max_steps = 1000;
  Boundary left = Boundary::Periodic, right = Boundary::Periodic,
           sides = Boundary::Periodic;
  // Discharge per unit width [m^2/s], velocity [m/s], depth [m].
  double discharge = 0, velocity = 0, outlet_depth = 1;
};
struct ShallowWaterSnapshot {
  int nx = 0, ny = 0;
  std::vector<double> depth, velocity_x, velocity_y, bed;
  std::vector<std::uint8_t> obstacles;
  double time = 0;
  std::uint64_t steps = 0;
};
enum class ShallowWaterScenario { Bump, Tide, Cylinder, Expansion };
inline constexpr const char* ShallowWaterScenarioNames[] = {
    "Channel bump", "Tidal wave", "Cylinder", "Sudden expansion"};
ShallowWaterSettings ShallowWaterParameters(ShallowWaterScenario scenario);
ShallowWaterSnapshot ShallowWaterInitialState(ShallowWaterScenario scenario);
}  // namespace domain
