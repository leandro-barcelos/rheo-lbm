#ifndef RHEOLBM_CAMERA_H
#define RHEOLBM_CAMERA_H

#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "rheo/domain/elevation.h"
#include "rheo/domain/lattice_editing.h"
#include "rheo/events/input_event.h"
#include "rheo/platform/window.h"

namespace renderer {

class Camera {
 public:
  Camera(Camera const&) = delete;
  Camera(Camera&&) = delete;
  Camera& operator=(Camera const&) = delete;
  Camera& operator=(Camera&&) = delete;

  explicit Camera(glm::vec3 position, platform::WindowSize initial_window_size);

  domain::Ray ScreenRay(double x, double y) const;
  void HandleInput(events::InputEvent const& event);

  [[nodiscard]] glm::mat4 ViewMatrix() const;
  [[nodiscard]] glm::mat4 ProjectionMatrix(float aspect_ratio,
                                           float near_plane = 0.1F) const;
  void InitTopView(glm::vec3 const& bounds_min, glm::vec3 const& bounds_max);
  [[nodiscard]] std::optional<glm::vec3> RaycastToTerrain(
      double xpos, double ypos,
      std::vector<domain::Elevation> const& elevation_samples,
      uint32_t dem_width, uint32_t dem_height, float step = 0.01F) const;
  [[nodiscard]] glm::vec3 Position() const { return position_; }
  [[nodiscard]] glm::vec3 Front() const { return front_; }
  [[nodiscard]] float GetZoom() const { return zoom_; }

  // Events' callbacks
  void OnMouseButtonPressedEvent(events::MouseButtonPressedEvent const& event);
  void OnMouseButtonReleasedEvent(
      events::MouseButtonReleasedEvent const& event);
  void OnMouseScrolledEvent(events::MouseScrolledEvent const& event);
  void OnMouseMovedEvent(events::MouseMovedEvent const& event);
  void OnWindowResizedEvent(events::WindowResizedEvent const& event);
  void OnKeyPressedEvent(events::KeyPressedEvent const& event);
  void OnKeyReleasedEvent(events::KeyReleasedEvent const& event);

 private:
  glm::vec3 position_;
  glm::vec3 front_;
  glm::vec3 up_;
  glm::vec3 right_;
  glm::vec3 world_up_;
  float movement_speed_{1.0F};
  float mouse_sensitivity_{1.0F};
  float zoom_{45.0F};
  float far_plane_{100.0F};

  [[nodiscard]] glm::vec3 ScreenToWorldPosition(double xpos, double ypos,
                                                float depth_ndc = 0.0F) const;
  [[nodiscard]] std::optional<glm::vec3> RaycastToPlane(
      double xpos, double ypos, float plane_y = 0.0F) const;

  // Camera Zoom
  bool is_control_pressed_{false};

  // Camera Pan
  bool mouse_left_button_held_{false};
  std::optional<glm::vec2> current_mouse_screen_position_;
  platform::WindowSize window_size_;
};

}  // namespace renderer

#endif  // !RHEOLBM_CAMERA_H
