#include "app_options.h"

namespace rheo {
std::expected<AppOptions, std::string> ParseAppOptions(
    std::span<std::string_view const> arguments) {
  AppOptions options;
  bool model_selected = false;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    auto argument = arguments[i];
    if (argument == "--help") {
      options.help = true;
    } else if (argument == "--model") {
      if (model_selected) {
        return std::unexpected("--model may only be specified once");
      }
      if (++i == arguments.size()) {
        return std::unexpected("--model requires 2d or 3d");
      }
      auto model = arguments[i];
      if (model == "2d") {
        options.model = SimulationModel::kShallowWater2D;
      } else if (model == "3d") {
        options.model = SimulationModel::kFreeSurface3D;
      } else {
        return std::unexpected("Invalid model: " + std::string(model));
      }
      model_selected = true;
    } else {
      return std::unexpected("Unknown argument: " + std::string(argument));
    }
  }
  return options;
}
}  // namespace rheo
