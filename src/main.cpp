#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#include "app_options.h"
#include "rheo_lbm_app.h"

int main(int argc, char** argv) {
  std::vector<std::string_view> arguments(argv + 1, argv + argc);
  auto options = rheo::ParseAppOptions(arguments);
  if (!options) {
    std::cerr << options.error() << '\n' << rheo::kAppUsage;
    return EXIT_FAILURE;
  }
  if (options->help) {
    std::cout << rheo::kAppUsage;
    return EXIT_SUCCESS;
  }
  rheo::RheoLBMApp app(options->model);
  app.Run();
  return EXIT_SUCCESS;
}
