#include "rheo/ui/shallow_water_interface.h"

#include "imgui.h"
#include "imgui_layer.h"
namespace ui {
class ShallowWaterInterface::Impl {
 public:
  ImGuiLayer layer;
  renderer::ShallowWaterMapOptions options;
  void Draw(application::ShallowWaterController& c,
            renderer::ShallowWaterRenderer& map) {
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Shallow water", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    int selected = static_cast<int>(c.Scenario());
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("Scenario", &selected, domain::ShallowWaterScenarioNames,
                     4)) {
      c.Select(static_cast<domain::ShallowWaterScenario>(selected));
      map.FitMap();
      options.vectors = selected >= 2;
    }
    ImGui::SameLine();
    if (ImGui::Button(c.Running() ? "Pause" : "Start / resume", {110, 0})) {
      if (c.Running())
        c.Pause();
      else
        c.Start();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) c.Reset();
    auto const& p = c.Parameters();
    auto const& s = c.Snapshot();
    ImGui::Text(
        "%d x %d | dx %.7g m | dt %.7g s | g %.4g m/s^2 | tau %.4g | Manning "
        "%.4g",
        p.nx, p.ny, p.dx, p.dt, p.gravity, p.tau, p.manning);
    ImGui::Text("Time %.5f / %.5f s | step %llu / %llu | %s", s.time,
                p.max_steps * p.dt, (unsigned long long)s.steps,
                (unsigned long long)p.max_steps,
                c.Running()              ? "Running"
                : s.steps == p.max_steps ? "Finished"
                                         : "Paused");
    switch (c.Scenario()) {
      case domain::ShallowWaterScenario::Bump:
        ImGui::TextUnformatted(
            "25 m channel | inlet q=4.42 m^2/s | outlet h=2 m | slip sides");
        break;
      case domain::ShallowWaterScenario::Tide:
        ImGui::TextUnformatted(
            "14 km channel | tidal inlet (Eq. 25) | downstream u=0 | slip "
            "sides");
        break;
      case domain::ShallowWaterScenario::Cylinder:
        ImGui::TextUnformatted(
            "4 x 2 m | radius 0.11 m | Q=0.248 m^3/s | outlet h=0.185 m | "
            "slope=-0.000625");
        break;
      case domain::ShallowWaterScenario::Expansion:
        ImGui::TextUnformatted(
            "2 x 1 m entrance; 4 x 3 m expansion | Q=0.032 m^3/s | outlet "
            "h=0.16 "
            "m | no-slip walls");
        break;
    }
    if (!c.Error().empty())
      ImGui::TextColored({1, .3f, .2f, 1}, "%s", c.Error().c_str());
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("Field", &options.field,
                 "Depth (m)\0Surface elevation (m)\0Speed (m/s)\0");
    ImGui::SameLine();
    ImGui::Checkbox("Velocity vectors", &options.vectors);
    ImGui::SameLine();
    if (ImGui::Button("Fit map")) {
      map.FitMap();
    }
    ImGui::TextUnformatted("Wheel: zoom | right drag: pan (over map)");
    map.DrawMap(s, p, options);
    ImGui::End();
  }
};
ShallowWaterInterface::ShallowWaterInterface()
    : impl_(std::make_unique<Impl>()) {}
ShallowWaterInterface::~ShallowWaterInterface() = default;
void ShallowWaterInterface::Init(const platform::Window& w,
                                 const graphics::GraphicsContext& c,
                                 const graphics::Device& d,
                                 const graphics::SwapChain& s) {
  impl_->layer.Init(w, c, d, s);
  ImGui::GetIO().IniFilename = nullptr;
}
void ShallowWaterInterface::BeginFrame() { impl_->layer.BeginFrame(); }
void ShallowWaterInterface::EndFrame() { impl_->layer.EndFrame(); }
void ShallowWaterInterface::Shutdown() { impl_->layer.Shutdown(); }
void ShallowWaterInterface::Draw(application::ShallowWaterController& c,
                                 renderer::ShallowWaterRenderer& r) {
  impl_->Draw(c, r);
}
void ShallowWaterInterface::Render(graphics::CommandList c) const {
  impl_->layer.Render(
      vk::CommandBuffer(static_cast<VkCommandBuffer>(c.native_handle)));
}
void ShallowWaterInterface::OnFrameResourcesChanged(std::uint32_t count) {
  impl_->layer.OnFrameResourcesChanged(count);
}
}  // namespace ui
