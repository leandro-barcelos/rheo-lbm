#include "camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <numbers>
#include <optional>
#include <type_traits>
#include <variant>

#include "rheo/events/mouse_codes.h"

renderer::Camera::Camera(glm::vec3 position,
                         platform::WindowSize initial_window_size)
    : position_(position),
      front_(0.0F, -1.0F, 0.0F),
      up_(0.0F, 0.0F, -1.0F),
      right_(1.0F, 0.0F, 0.0F),
      world_up_(0.0F, 1.0F, 0.0F),
      window_size_(initial_window_size) {}

void renderer::Camera::HandleInput(events::InputEvent const& event) {
  std::visit(
      [this](auto const& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, events::MouseButtonPressedEvent>) {
          OnMouseButtonPressedEvent(value);
        } else if constexpr (std::is_same_v<T,
                                            events::MouseButtonReleasedEvent>) {
          OnMouseButtonReleasedEvent(value);
        } else if constexpr (std::is_same_v<T, events::MouseScrolledEvent>) {
          OnMouseScrolledEvent(value);
        } else if constexpr (std::is_same_v<T, events::MouseMovedEvent>) {
          OnMouseMovedEvent(value);
        } else if constexpr (std::is_same_v<T, events::WindowResizedEvent>) {
          OnWindowResizedEvent(value);
        } else if constexpr (std::is_same_v<T, events::KeyPressedEvent>) {
          OnKeyPressedEvent(value);
        } else if constexpr (std::is_same_v<T, events::KeyReleasedEvent>) {
          OnKeyReleasedEvent(value);
        }
      },
      event);
}

void renderer::Camera::OnMouseButtonPressedEvent(
    events::MouseButtonPressedEvent const& event) {
  if (event.button == events::MouseCode::kButtonLeft) {
    mouse_left_button_held_ = true;
  }
}

void renderer::Camera::OnMouseButtonReleasedEvent(
    events::MouseButtonReleasedEvent const& event) {
  if (event.button == events::MouseCode::kButtonLeft) {
    mouse_left_button_held_ = false;
    current_mouse_screen_position_ = std::nullopt;
  }
}

void renderer::Camera::OnMouseScrolledEvent(
    events::MouseScrolledEvent const& event) {
  if (!is_control_pressed_) {
    return;
  }

  constexpr float kMinZoom = 5.0F;
  constexpr float kMaxZoom = 120.0F;

  zoom_ -= event.y_offset * mouse_sensitivity_;
  zoom_ = std::clamp(zoom_, kMinZoom, kMaxZoom);
}

void renderer::Camera::OnMouseMovedEvent(events::MouseMovedEvent const& event) {
  if (!mouse_left_button_held_) {
    return;
  }

  if (!current_mouse_screen_position_.has_value()) {
    current_mouse_screen_position_ = glm::vec2(event.x, event.y);
    return;
  }

  auto prev_mouse_screen_position = *current_mouse_screen_position_;

  current_mouse_screen_position_ = glm::vec2(event.x, event.y);

  auto pan_anchor_world = RaycastToPlane(prev_mouse_screen_position[0],
                                         prev_mouse_screen_position[1]);
  auto current_position_world =
      RaycastToPlane(current_mouse_screen_position_.value()[0],
                     current_mouse_screen_position_.value()[1]);

  if (!pan_anchor_world.has_value() || !current_position_world.has_value()) {
    return;
  }

  auto delta = *pan_anchor_world - *current_position_world;
  position_ += delta;
}

void renderer::Camera::OnWindowResizedEvent(
    events::WindowResizedEvent const& event) {
  if (event.width <= 0 || event.height <= 0) {
    return;
  }

  window_size_ = {.width = event.width, .height = event.height};
}

void renderer::Camera::OnKeyPressedEvent(events::KeyPressedEvent const& event) {
  switch (event.key) {
    case events::kLeftControl:
    case events::kRightControl:
      is_control_pressed_ = true;
      break;
    default:
      break;
  }
}

void renderer::Camera::OnKeyReleasedEvent(
    events::KeyReleasedEvent const& event) {
  switch (event.key) {
    case events::kLeftControl:
    case events::kRightControl:
      is_control_pressed_ = false;
      break;
    default:
      break;
  }
}

glm::mat4 renderer::Camera::ViewMatrix() const {
  return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 renderer::Camera::ProjectionMatrix(float aspect_ratio,
                                             float near_plane) const {
  // With a positive Vulkan viewport height, camera +Y (-Z in the world)
  // points down on screen. Keep this sign so DEM north (+Z) is at the top
  // while east (+X) stays on the right.
  return glm::perspectiveRH_ZO(glm::radians(zoom_), aspect_ratio, near_plane,
                              far_plane_);
}

void renderer::Camera::InitTopView(glm::vec3 const& bounds_min,
                                   glm::vec3 const& bounds_max) {
  constexpr float kHeightMultiplier = 1.1F;
  constexpr float kFramePadding = 0.55F;

  glm::vec3 const bounds_center = (bounds_min + bounds_max) * 0.5F;
  glm::vec3 const bounds_size = bounds_max - bounds_min;
  float const max_dimension = std::max(bounds_size[0], bounds_size[2]);

  position_ = bounds_center;
  position_[1] = bounds_max[1] + (max_dimension * kHeightMultiplier);

  // Align axes for a straight top-down view.
  front_ = {0.0F, -1.0F, 0.0F};
  up_ = {0.0F, 0.0F, -1.0F};
  right_ = {1.0F, 0.0F, 0.0F};

  float const distance = std::max(position_[1] - bounds_center[1], 1e-4F);
  float const required_fov =
      2.0F * std::atan((max_dimension * kFramePadding) / distance) *
      (180.0F / std::numbers::pi_v<float>);
  zoom_ = std::clamp(required_fov, 10.0F, 120.0F);

  far_plane_ = std::max(position_[1] - bounds_min[1] + max_dimension, 100.0F);
}

std::optional<glm::vec3> renderer::Camera::RaycastToTerrain(
    double xpos, double ypos,
    std::vector<domain::Elevation> const& elevation_samples, uint32_t dem_width,
    uint32_t dem_height, float step) const {
  auto near_point = ScreenToWorldPosition(xpos, ypos, 0.0F);
  auto far_point = ScreenToWorldPosition(xpos, ypos, 1.0F);

  if (elevation_samples.empty() || dem_width == 0 || dem_height == 0 ||
      step <= 0.0F) {
    return std::nullopt;
  }

  glm::vec3 const ray_vector = far_point - near_point;
  float const ray_length = glm::length(ray_vector);
  if (ray_length <= 0.0F) {
    return std::nullopt;
  }

  glm::vec3 const ray_dir = ray_vector / ray_length;

  for (uint32_t step_index = 0;; ++step_index) {
    float const travelled = static_cast<float>(step_index) * step;
    if (travelled > ray_length) {
      break;
    }

    glm::vec3 const current_point = near_point + travelled * ray_dir;
    float const terrain_height = domain::Elevation::GetElevation(
        elevation_samples, dem_width, dem_height, current_point);

    if (current_point[1] <= terrain_height) {
      return current_point;
    }
  }

  return std::nullopt;
}

glm::vec3 renderer::Camera::ScreenToWorldPosition(double xpos, double ypos,
                                                  float depth_ndc) const {
  int const window_width = std::max(window_size_.width, 1);
  int const window_height = std::max(window_size_.height, 1);
  float const aspect =
      static_cast<float>(window_width) / static_cast<float>(window_height);

  // Reconstruct NDC — Vulkan is [-1,1] x [-1,1] x [0,1]
  float const ndc_x = ((static_cast<float>(std::floor(xpos)) + 0.5F) /
                       static_cast<float>(window_width) * 2.0F) -
                      1.0F;
  float const ndc_y = ((static_cast<float>(std::floor(ypos)) + 0.5F) /
                       static_cast<float>(window_height) * 2.0F) -
                      1.0F;

  glm::mat4 const inv = glm::inverse(ProjectionMatrix(aspect) * ViewMatrix());

  glm::vec4 const clip{ndc_x, ndc_y, depth_ndc, 1.0F};
  glm::vec4 world = inv * clip;
  world /= world[3];

  return glm::vec3{world};
}

std::optional<glm::vec3> renderer::Camera::RaycastToPlane(double xpos,
                                                          double ypos,
                                                          float plane_y) const {
  auto near_point = ScreenToWorldPosition(xpos, ypos, 0.0F);
  auto far_point = ScreenToWorldPosition(xpos, ypos, 1.0F);

  glm::vec3 const ray_dir = glm::normalize(far_point - near_point);

  // Quit if ray and plane are parallel
  if (std::abs(ray_dir[1]) < 1e-6F) {
    return std::nullopt;
  }

  float const plane_depth = (plane_y - near_point[1]) / ray_dir[1];

  // Intersection is behind the camera
  if (plane_depth < 0.0F) {
    return std::nullopt;
  }

  return near_point + plane_depth * ray_dir;
}
