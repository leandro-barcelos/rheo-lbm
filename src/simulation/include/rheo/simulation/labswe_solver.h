#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "rheo/domain/shallow_water.h"

namespace simulation {
inline constexpr int kNumSpeeds = 9;
inline constexpr int kVelocityVectorX[9] = {0, 1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr int kVelocityVectorY[9] = {0, 0, 1, 1, 1, 0, -1, -1, -1};
using domain::Boundary;
using domain::ShallowWaterSettings;
using domain::ShallowWaterSnapshot;
class LabsweSolver {
 public:
  explicit LabsweSolver(ShallowWaterSettings settings);
  void Initialize(ShallowWaterSnapshot state);
  void Step();
  void Run(int steps);
  const ShallowWaterSnapshot& Snapshot() const { return state_; }
  const ShallowWaterSettings& Settings() const { return settings_; }

 private:
  std::array<double, 9> Equilibrium(double h, double u, double v) const;
  void Macroscopic();
  void OpenBoundary(int x, Boundary boundary);
  ShallowWaterSettings settings_;
  ShallowWaterSnapshot state_;
  std::array<std::vector<double>, 9> cells_, post_, next_;
  std::vector<double> gradient_x_, gradient_y_;
};
}  // namespace simulation
