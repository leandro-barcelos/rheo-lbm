#include "rheo/simulation/labswe_solver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace simulation {
namespace {
constexpr int opposite[9] = {0, 5, 6, 7, 8, 1, 2, 3, 4};
}
LabsweSolver::LabsweSolver(ShallowWaterSettings s) : settings_(s) {
  if (s.nx < 3 || s.ny < 3 || !std::isfinite(s.dx) || s.dx <= 0 ||
      !std::isfinite(s.dt) || s.dt <= 0 || !std::isfinite(s.tau) ||
      s.tau <= .5 || !std::isfinite(s.gravity) || s.gravity <= 0 ||
      !std::isfinite(s.manning) || s.manning < 0 ||
      !std::isfinite(s.discharge) || !std::isfinite(s.velocity) ||
      !std::isfinite(s.outlet_depth) || s.outlet_depth <= 0 ||
      std::size_t(s.nx) * s.ny > std::numeric_limits<int>::max())
    throw std::invalid_argument("Invalid shallow-water parameters");
  ShallowWaterSnapshot a;
  a.nx = s.nx;
  a.ny = s.ny;
  const auto n = std::size_t(s.nx) * s.ny;
  a.depth.assign(n, s.outlet_depth);
  a.velocity_x.assign(n, 0);
  a.velocity_y.assign(n, 0);
  a.bed.assign(n, 0);
  a.obstacles.assign(n, 0);
  Initialize(std::move(a));
}
std::array<double, 9> LabsweSolver::Equilibrium(double h, double u,
                                                double v) const {
  // Zhou (2002), Eq. 15, expressed using dimensionless u/e and v/e.
  const double e = settings_.dx / settings_.dt;
  u /= e;
  v /= e;
  const double gh = settings_.gravity * h / (e * e), vv = u * u + v * v;
  std::array<double, 9> f{};
  f[0] = h * (1 - 5 * gh / 6 - 2 * vv / 3);
  for (int a = 1; a < 9; ++a) {
    const double c = kVelocityVectorX[a] * u + kVelocityVectorY[a] * v;
    f[a] = ((a & 1) ? 1. : .25) * h * (gh / 6 + c / 3 + c * c / 2 - vv / 6);
  }
  return f;
}
void LabsweSolver::Initialize(ShallowWaterSnapshot a) {
  const auto n = std::size_t(settings_.nx) * settings_.ny;
  if (a.nx != settings_.nx || a.ny != settings_.ny || a.depth.size() != n ||
      a.bed.size() != n || a.velocity_x.size() != n ||
      a.velocity_y.size() != n || a.obstacles.size() != n)
    throw std::invalid_argument("Invalid shallow-water field dimensions");
  for (std::size_t i = 0; i < n; ++i)
    if (!std::isfinite(a.depth[i]) || a.depth[i] <= 0 ||
        !std::isfinite(a.bed[i]) || !std::isfinite(a.velocity_x[i]) ||
        !std::isfinite(a.velocity_y[i]))
      throw std::invalid_argument("Invalid initial shallow-water field");
  state_ = std::move(a);
  state_.time = 0;
  state_.steps = 0;
  gradient_x_.resize(n);
  gradient_y_.resize(n);
  for (int a = 0; a < 9; ++a) {
    cells_[a].resize(n);
    post_[a].resize(n);
    next_[a].resize(n);
  }
  for (int y = 0; y < state_.ny; ++y)
    for (int x = 0; x < state_.nx; ++x) {
      int i = x + y * state_.nx;
      auto f = Equilibrium(state_.depth[i], state_.velocity_x[i],
                           state_.velocity_y[i]);
      for (int a = 0; a < 9; ++a) cells_[a][i] = f[a];
      int l = std::max(0, x - 1), r = std::min(state_.nx - 1, x + 1),
          b = std::max(0, y - 1), t = std::min(state_.ny - 1, y + 1);
      gradient_x_[i] =
          (state_.bed[r + y * state_.nx] - state_.bed[l + y * state_.nx]) /
          ((r - l) * settings_.dx);
      gradient_y_[i] =
          (state_.bed[x + t * state_.nx] - state_.bed[x + b * state_.nx]) /
          ((t - b) * settings_.dx);
    }
}
void LabsweSolver::OpenBoundary(int x, Boundary bc) {
  if (bc == Boundary::Periodic || bc == Boundary::NoSlip ||
      bc == Boundary::Slip)
    return;
  const int nx = state_.nx;
  const double e = settings_.dx / settings_.dt;
  for (int y = 0; y < state_.ny; ++y) {
    int i = x + y * nx, j = i + (x == 0 ? 1 : -1);
    if (state_.obstacles[i]) continue;
    if (bc == Boundary::ZeroGradient) {
      for (int a = 0; a < 9; ++a) next_[a][i] = next_[a][j];
      continue;
    }
    // Extrapolate non-equilibrium stress from the adjacent fluid cell.
    // Discharge: zero depth gradient, prescribed hu, and v=0.
    // Depth: prescribed h and zero velocity gradient. Keeping the stress
    // avoids resetting the viscous part when imposing the macroscopic data.
    if (bc == Boundary::Discharge || bc == Boundary::Depth) {
      double h = 0, u = 0, v = 0;
      for (int a = 0; a < 9; ++a) {
        h += next_[a][j];
        u += next_[a][j] * kVelocityVectorX[a];
        v += next_[a][j] * kVelocityVectorY[a];
      }
      auto interior = Equilibrium(h, e * u / h, e * v / h);
      auto imposed =
          bc == Boundary::Discharge
              ? Equilibrium(h, settings_.discharge / h, 0)
              : Equilibrium(settings_.outlet_depth, e * u / h, e * v / h);
      for (int a = 0; a < 9; ++a)
        next_[a][i] = imposed[a] + next_[a][j] - interior[a];
      continue;
    }
    auto f = [&](int a) -> double& { return next_[a][i]; };
    double h = 0, u = 0;
    if (x == 0) {
      const double known = f(0) + f(3) + f(7) + 2 * (f(5) + f(6) + f(4));
      if (bc == Boundary::Velocity) {
        u = settings_.velocity / e;
        h = known / (1 - u);
      } else {
        h = bc == Boundary::Tide ? 60.5 + 4 -
                                       4 * std::sin(std::numbers::pi *
                                                    (4 * (state_.steps + 1) *
                                                         settings_.dt / 86400 +
                                                     .5))
                                 : settings_.outlet_depth;
        u = 1 - known / h;
      }
      f(1) = f(5) + 2 * h * u / 3;
      f(2) = f(6) + h * u / 6 + (f(7) - f(3)) / 2;
      f(8) = f(4) + h * u / 6 + (f(3) - f(7)) / 2;
    } else {
      const double known = f(0) + f(3) + f(7) + 2 * (f(1) + f(2) + f(8));
      if (bc == Boundary::Velocity) {
        u = settings_.velocity / e;
        h = known / (1 + u);
      } else {
        h = settings_.outlet_depth;
        u = known / h - 1;
      }
      f(5) = f(1) - 2 * h * u / 3;
      f(6) = f(2) - h * u / 6 + (f(3) - f(7)) / 2;
      f(4) = f(8) - h * u / 6 + (f(7) - f(3)) / 2;
    }
  }
}
void LabsweSolver::Macroscopic() {
  int bad = 0;
  const int n = state_.nx * state_.ny;
  const double e = settings_.dx / settings_.dt;
#pragma omp parallel for reduction(| : bad) schedule(static) if (n > 20000)
  for (int i = 0; i < n; ++i) {
    if (state_.obstacles[i]) {
      state_.velocity_x[i] = state_.velocity_y[i] = 0;
      continue;
    }
    double h = 0, u = 0, v = 0;
    for (int a = 0; a < 9; ++a) {
      double f = cells_[a][i];
      h += f;
      u += f * kVelocityVectorX[a];
      v += f * kVelocityVectorY[a];
    }
    u = e * u / h;
    v = e * v / h;
    if (!std::isfinite(h) || h <= 1e-12 || !std::isfinite(u) ||
        !std::isfinite(v))
      bad = 1;
    state_.depth[i] = h;
    state_.velocity_x[i] = u;
    state_.velocity_y[i] = v;
  }
  if (bad)
    throw std::runtime_error("Invalid depth or velocity at step " +
                             std::to_string(state_.steps) +
                             "; reset the scenario.");
}
void LabsweSolver::Step() {
  const int nx = state_.nx, ny = state_.ny, n = nx * ny;
  const double e = settings_.dx / settings_.dt;
#pragma omp parallel for schedule(static) if (n > 20000)
  for (int i = 0; i < n; ++i) {
    double h = state_.depth[i], u = state_.velocity_x[i],
           v = state_.velocity_y[i];
    auto eq = Equilibrium(h, u, v);
    double friction = settings_.manning == 0
                          ? 0
                          : settings_.gravity * settings_.manning *
                                settings_.manning * std::hypot(u, v) /
                                std::cbrt(h);
    double fx = -settings_.gravity * h * gradient_x_[i] - friction * u;
    double fy = -settings_.gravity * h * gradient_y_[i] - friction * v;
    // Eq. 7 with N_alpha=6; Eq. 16 supplies slope and Manning forces.
    for (int a = 0; a < 9; ++a)
      post_[a][i] = cells_[a][i] + (eq[a] - cells_[a][i]) / settings_.tau +
                    settings_.dt / (6 * e) *
                        (kVelocityVectorX[a] * fx + kVelocityVectorY[a] * fy);
  }
#pragma omp parallel for schedule(static) if (n > 20000)
  for (int y = 0; y < ny; ++y)
    for (int x = 0; x < nx; ++x) {
      int i = x + y * nx;
      for (int a = 0; a < 9; ++a) {
        int sx = x - kVelocityVectorX[a], sy = y - kVelocityVectorY[a],
            direction = a;
        bool bounce = false;
        if (sy < 0 || sy >= ny) {
          if (settings_.sides == Boundary::Periodic)
            sy = (sy + ny) % ny;
          else if (settings_.sides == Boundary::NoSlip)
            bounce = true;
          else {
            sy = y;
            for (int b = 0; b < 9; ++b)
              if (kVelocityVectorX[b] == kVelocityVectorX[a] &&
                  kVelocityVectorY[b] == -kVelocityVectorY[a])
                direction = b;
          }
        }
        if (sx < 0 || sx >= nx) {
          auto bc = sx < 0 ? settings_.left : settings_.right;
          if (bc == Boundary::Periodic)
            sx = (sx + nx) % nx;
          else if (bc == Boundary::NoSlip)
            bounce = true;
          else if (bc == Boundary::Slip) {
            sx = x;
            for (int b = 0; b < 9; ++b)
              if (kVelocityVectorX[b] == -kVelocityVectorX[direction] &&
                  kVelocityVectorY[b] == kVelocityVectorY[direction]) {
                direction = b;
                break;
              }
          } else
            sx = x;
        }
        int j = std::clamp(sx, 0, nx - 1) + std::clamp(sy, 0, ny - 1) * nx;
        next_[a][i] = bounce || state_.obstacles[j] ? post_[opposite[a]][i]
                                                    : post_[direction][j];
      }
    }
  OpenBoundary(0, settings_.left);
  OpenBoundary(nx - 1, settings_.right);
  cells_.swap(next_);
  ++state_.steps;
  state_.time = state_.steps * settings_.dt;
  Macroscopic();
}
void LabsweSolver::Run(int steps) {
  for (int i = 0; i < steps; ++i) Step();
}
}  // namespace simulation
