#ifndef RHEO_PLATFORM_WINDOW_H
#define RHEO_PLATFORM_WINDOW_H

#include <vector>

namespace events {
class InputQueue;
}

namespace platform {

struct WindowSize {
  int width = 0;
  int height = 0;
};

struct WindowProperties {
  int width = 1280;
  int height = 720;
  const char* title = "Rheo LBM";
};

class Window {
 public:
  Window(Window const&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window const&) = delete;
  Window& operator=(Window&&) = delete;

  Window(WindowProperties properties, events::InputQueue& input_queue);
  ~Window();

  [[nodiscard]] bool ShouldClose() const;
  void RequestClose();
  [[nodiscard]] WindowSize Size() const;
  [[nodiscard]] WindowSize LogicalSize() const;
  [[nodiscard]] void* NativeHandle() const { return window_; }
  [[nodiscard]] events::InputQueue& InputEvents() const {
    return *input_queue_;
  }

  static void PollEvents();
  static void WaitEvents();
  [[nodiscard]] static double TimeSeconds();
  [[nodiscard]] static std::vector<const char*> RequiredGraphicsExtensions();

 private:
  void* window_ = nullptr;
  events::InputQueue* input_queue_ = nullptr;

  static void ErrorCallback(int error, const char* description);
};

}  // namespace platform

#endif  // RHEO_PLATFORM_WINDOW_H
