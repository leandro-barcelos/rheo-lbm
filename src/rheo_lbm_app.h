#pragma once
#include <memory>
namespace rheo {
class RheoLBMApp {
 public:
  RheoLBMApp();
  ~RheoLBMApp();
  RheoLBMApp(const RheoLBMApp&) = delete;
  RheoLBMApp& operator=(const RheoLBMApp&) = delete;
  void Run();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace rheo
