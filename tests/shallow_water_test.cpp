#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string_view>

#include "rheo/application/shallow_water_controller.h"
#ifdef _OPENMP
#include <omp.h>
#endif
using namespace simulation;
using namespace domain;
using namespace application;
void Check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void Near(double a, double b, double tolerance, const char* message) {
  Check(std::abs(a - b) <= tolerance, message);
}
void Horizons() {
  for (int k = 0; k < 4; ++k) {
    auto scenario = static_cast<ShallowWaterScenario>(k);
    ShallowWaterSession session(scenario);
    const auto count = session.solver.Settings().max_steps;
    auto start = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < count; ++i) {
      session.solver.Step();
      if (i % 10000 == 9999)
        std::cout << ShallowWaterScenarioNames[k] << " step " << i + 1
                  << std::endl;
    }
    auto const& s = session.solver.Snapshot();
    Near(s.time, count * session.solver.Settings().dt, 1e-10, "horizon time");
    double min = 1e99, max = 0;
    for (std::size_t i = 0; i < s.depth.size(); ++i)
      if (!s.obstacles[i]) {
        Check(std::isfinite(s.depth[i]) && s.depth[i] > 0, "invalid horizon");
        min = std::min(min, s.depth[i]);
        max = std::max(max, s.depth[i]);
      }
    std::cout << ShallowWaterScenarioNames[k] << ": depth " << min << " .. "
              << max << ", elapsed "
              << std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - start)
                     .count()
              << " s" << std::endl;
  }
}
int main(int argc, char** argv) {
  try {
#ifdef _OPENMP
    omp_set_num_threads(4);
#endif
    if (argc > 1 && std::string_view(argv[1]) == "--horizons") {
      Horizons();
      return 0;
    }
    ShallowWaterSettings p;
    LabsweSolver a(p), b(p);
    auto initial = a.Snapshot();
    for (std::size_t i = 0; i < initial.depth.size(); ++i) {
      initial.velocity_x[i] = .1;
      initial.velocity_y[i] = -.05;
    }
    a.Initialize(initial);
    a.Run(200);
    for (auto h : a.Snapshot().depth) Near(h, 1, 1e-12, "uniform depth");
    for (auto u : a.Snapshot().velocity_x)
      Near(u, .1, 1e-12, "uniform momentum");
    initial.depth[p.nx / 2 + (p.ny / 2) * p.nx] += .01;
    initial.velocity_x.assign(initial.depth.size(), 0);
    initial.velocity_y.assign(initial.depth.size(), 0);
    a.Initialize(initial);
    b.Initialize(initial);
    a.Run(100);
    for (int i = 0; i < 100; ++i) b.Step();
    Check(a.Snapshot().depth == b.Snapshot().depth, "Run equivalence");
    Near(std::accumulate(a.Snapshot().depth.begin(), a.Snapshot().depth.end(),
                         0.),
         std::accumulate(initial.depth.begin(), initial.depth.end(), 0.), 1e-9,
         "periodic mass conservation");
    for (int y = 1; y < p.ny; ++y)
      for (int x = 1; x < p.nx; ++x)
        Near(a.Snapshot().depth[x + y * p.nx],
             a.Snapshot().depth[(p.nx - x) + (p.ny - y) * p.nx], 1e-12,
             "reflection symmetry");
    initial.obstacles[p.nx / 2 + p.nx * (p.ny / 2)] = 1;
    a.Initialize(initial);
    a.Run(100);
    double mass0 = 0, mass1 = 0;
    for (std::size_t i = 0; i < initial.depth.size(); ++i)
      if (!initial.obstacles[i]) {
        mass0 += initial.depth[i];
        mass1 += a.Snapshot().depth[i];
      }
    Near(mass0, mass1, 1e-9, "obstacle mass conservation");
    Near(a.Snapshot().velocity_x[p.nx / 2 + p.nx * (p.ny / 2)], 0, 0,
         "obstacle velocity");
    for (auto side : {Boundary::Slip, Boundary::NoSlip}) {
      p.left = Boundary::Discharge;
      p.right = Boundary::Depth;
      p.sides = side;
      p.discharge = .02;
      LabsweSolver flow(p);
      flow.Run(100);
      for (int y = 1; y < p.ny - 1; ++y) {
        int i = y * p.nx;
        Near(flow.Snapshot().depth[i] * flow.Snapshot().velocity_x[i], .02,
             1e-12, "inlet discharge");
        Near(flow.Snapshot().depth[i + p.nx - 1], 1, 1e-12, "outlet depth");
      }
    }
    p.left = p.right = Boundary::ZeroGradient;
    p.sides = Boundary::Slip;
    LabsweSolver zero(p);
    zero.Run(50);
    for (auto h : zero.Snapshot().depth) Near(h, 1, 1e-12, "zero gradient");
    p.left = Boundary::Velocity;
    p.right = Boundary::Depth;
    p.velocity = .03;
    LabsweSolver velocity(p);
    velocity.Run(50);
    Near(velocity.Snapshot().velocity_x[p.nx], .03, 1e-12, "velocity inlet");
    // Eq. 16: a linear bed generates the prescribed acceleration, including
    // the one-sided gradients at both edges. Manning damps uniform motion.
    ShallowWaterSettings force_parameters;
    LabsweSolver slope(force_parameters);
    auto sloped = slope.Snapshot();
    for (int y = 0; y < sloped.ny; ++y)
      for (int x = 0; x < sloped.nx; ++x)
        sloped.bed[x + y * sloped.nx] = -.001 * x * force_parameters.dx;
    slope.Initialize(sloped);
    slope.Step();
    for (double u : slope.Snapshot().velocity_x)
      Near(u, .001 * force_parameters.gravity * force_parameters.dt, 1e-13,
           "bed slope acceleration");
    force_parameters.manning = .03;
    LabsweSolver friction(force_parameters);
    auto moving = friction.Snapshot();
    moving.velocity_x.assign(moving.depth.size(), .2);
    friction.Initialize(moving);
    friction.Step();
    const double expected_velocity = .2 - force_parameters.dt *
                                              force_parameters.gravity * .03 *
                                              .03 * .2 * .2;
    for (double u : friction.Snapshot().velocity_x)
      Near(u, expected_velocity, 1e-13, "Manning deceleration");
    // Invalid evolved states must fail explicitly, never clamp or continue.
    moving.velocity_x.assign(moving.depth.size(), 1e200);
    friction.Initialize(moving);
    bool failed_step = false;
    try {
      friction.Step();
    } catch (const std::runtime_error&) {
      failed_step = true;
    }
    Check(failed_step, "invalid evolution stops");
    bool rejected = false;
    try {
      auto invalid = p;
      invalid.dt = 0;
      LabsweSolver bad(invalid);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    Check(rejected, "parameter validation");
    rejected = false;
    try {
      initial.depth[0] = NAN;
      a.Initialize(initial);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    Check(rejected, "initial state validation");
    constexpr int expected_nx[] = {500, 800, 600, 120};
    constexpr int expected_ny[] = {50, 3, 300, 60};
    constexpr double expected_dx[] = {.05, 17.5, 4. / 600, .05};
    constexpr double expected_dt[] = {.05 / 15, .0875, .00145, .025};
    constexpr double expected_tau[] = {1.5, .6, 1.982, 1};
    constexpr unsigned long long expected_steps[] = {20000, 104200, 40000,
                                                     10000};
    for (int k = 0; k < 4; ++k) {
      auto scenario = static_cast<ShallowWaterScenario>(k);
      auto settings = ShallowWaterParameters(scenario);
      auto s = ShallowWaterInitialState(scenario);
      Check(settings.nx == expected_nx[k] && settings.ny == expected_ny[k],
            "specified grid");
      Near(settings.dx, expected_dx[k], 0, "specified spacing");
      Near(settings.dt, expected_dt[k], 0, "specified time step");
      Near(settings.tau, expected_tau[k], 0, "specified relaxation");
      Check(settings.max_steps == expected_steps[k], "specified horizon");
      Check(s.nx == settings.nx && s.ny == settings.ny, "preset dimensions");
      for (auto u : s.velocity_x) Near(u, 0, 0, "rest initialization");
      if (k == 0) {
        Near(s.bed[200], .2, 1e-14, "bump maximum");
        Near(settings.discharge, 4.42, 0, "bump discharge");
      }
      if (k == 1) {
        Near(s.depth[0], 60.5, 1e-12, "tidal origin");
        Near(settings.dt * settings.max_steps, 9117.5, 1e-10, "tidal horizon");
      }
      if (k == 2) {
        Check(s.obstacles[300 + 150 * 600], "cylinder center");
        Near(settings.manning, .012, 0, "Manning");
      }
      if (k == 3) {
        Check(s.obstacles[0] && !s.obstacles[20 * 120] && !s.obstacles[40],
              "expansion geometry");
        Check(std::count(s.obstacles.begin(), s.obstacles.end(), 1) == 1600,
              "expansion wall area");
      }
      double surface = s.depth[0] + s.bed[0];
      for (std::size_t i = 0; i < s.depth.size(); ++i)
        Near(s.depth[i] + s.bed[i], surface, 1e-12, "level initial surface");
      ShallowWaterSession session(scenario);
      session.solver.Run(20);
    }
    ShallowWaterController controller;
    Check(controller.Scenario() == ShallowWaterScenario::Expansion &&
              !controller.Running(),
          "default paused expansion");
    controller.Advance(2);
    Check(controller.Snapshot().steps == 0, "paused does not advance");
    controller.Start();
    controller.Advance(5);
    Near(controller.Snapshot().time, .125, 1e-14, "physical time");
    controller.Pause();
    controller.Advance(5);
    Check(controller.Snapshot().steps == 5, "pause");
    controller.Start();
    controller.Advance(5);
    Check(controller.Snapshot().steps == 10, "resume");
    controller.Reset();
    Check(!controller.Running() && controller.Snapshot().steps == 0, "reset");
    controller.Select(ShallowWaterScenario::Bump);
    Check(controller.Snapshot().nx == 500 && !controller.Running(),
          "selection");
    controller.Select(ShallowWaterScenario::Expansion);
    controller.Start();
    controller.Advance(10001);
    Check(controller.Error().empty() && !controller.Running() &&
              controller.Snapshot().steps == 10000,
          "horizon stop");
    std::cout << "Shallow-water checks passed\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
