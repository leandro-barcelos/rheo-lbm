#ifndef RHEO_APP_OPTIONS_H
#define RHEO_APP_OPTIONS_H

#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "simulation_model.h"

namespace rheo {
struct AppOptions {
  SimulationModel model = SimulationModel::kFreeSurface3D;
  bool help = false;
} __attribute__((aligned(2)));

inline constexpr std::string_view kAppUsage =
    "Usage: rheo-lbm [--model 2d|3d] [--help]\n"
    "  --model 2d  Shallow water 2D\n"
    "  --model 3d  Free surface 3D (default)\n";

std::expected<AppOptions, std::string> ParseAppOptions(
    std::span<std::string_view const> arguments);
}  // namespace rheo

#endif  // RHEO_APP_OPTIONS_H
