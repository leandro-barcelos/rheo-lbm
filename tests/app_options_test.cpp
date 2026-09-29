#include "app_options.h"

#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "model_runners.h"

namespace {
int runs_2d = 0;
int runs_3d = 0;

void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

auto Parse(std::initializer_list<std::string_view> arguments) {
  return rheo::ParseAppOptions({arguments.begin(), arguments.size()});
}
}  // namespace

// Exercise the real dispatcher without opening a window or creating a device.
void rheo::RunFreeSurfaceApp() { ++runs_3d; }
void rheo::RunShallowWaterApp() { ++runs_2d; }

int main() {
  try {
    using rheo::SimulationModel;
    auto defaults = Parse({});
    Check(defaults && defaults->model == SimulationModel::kFreeSurface3D &&
              !defaults->help,
          "default model must be 3D");
    auto two = Parse({"--model", "2d"});
    Check(two && two->model == SimulationModel::kShallowWater2D && !two->help,
          "select 2D");
    auto three = Parse({"--model", "3d"});
    Check(
        three && three->model == SimulationModel::kFreeSurface3D && !three->help,
        "select 3D");
    auto help = Parse({"--help"});
    Check(help && help->help, "help without a model");
    for (auto arguments :
         {std::initializer_list<std::string_view>{"--help", "--model", "2d"},
          std::initializer_list<std::string_view>{"--model", "2d", "--help"}}) {
      auto options = Parse(arguments);
      Check(options && options->help &&
                options->model == SimulationModel::kShallowWater2D,
            "help with model in either order");
    }
    for (auto arguments :
         {std::initializer_list<std::string_view>{"--model"},
          std::initializer_list<std::string_view>{"--model", "4d"},
          std::initializer_list<std::string_view>{"--model", "--help"},
          std::initializer_list<std::string_view>{"--unknown"},
          std::initializer_list<std::string_view>{"2d"},
          std::initializer_list<std::string_view>{"--model", "2d", "--model",
                                                  "3d"},
          std::initializer_list<std::string_view>{"--model", "2d", "--model",
                                                  "2d"},
          std::initializer_list<std::string_view>{"--help", "--unknown"}}) {
      auto options = Parse(arguments);
      Check(!options && !options.error().empty(), "reject invalid arguments");
    }

    rheo::RheoLBMApp default_app;
    default_app.Run();
    Check(runs_3d == 1 && runs_2d == 0, "default dispatch initializes only 3D");
    rheo::RheoLBMApp(two->model).Run();
    Check(runs_3d == 1 && runs_2d == 1, "2D dispatch initializes only 2D");
    rheo::RheoLBMApp(three->model).Run();
    Check(runs_3d == 2 && runs_2d == 1, "3D dispatch initializes only 3D");
    std::cout << "Application options and model dispatch passed\n";
  } catch (std::exception const& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
