#include "rheo/simulation/simulation_session.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

#include "lattice_editor.h"
#include "lattice_initializer.h"
#include "lbm_solver.h"
namespace simulation {
class SimulationSession::Impl {
 public:
  Impl(graphics::Device const& device, graphics::CommandPools const& pools,
       graphics::FrameSync& sync)
      : device_(device), pools_(pools), sync_(sync) {}
  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings,
      std::optional<domain::LatticeEdits> const& edits) {
    if (!dem) return std::unexpected("Import a valid DEM first");
    auto definition = domain::DefineLattice(*dem, settings);
    if (!definition) return std::unexpected(definition.error());
    try {
      std::uint64_t signal = 0;
      // Older frames must finish before the shared timeline advances on
      // compute.
      device_.LogicalDevice().waitIdle();
      domain::CellTypes base;
      auto next = LatticeInitializer::Build(device_, pools_, sync_, *dem,
                                            *definition, signal, base);
      auto fingerprint = domain::DemFingerprint(*dem);
      auto current = base;
      if (edits) {
        auto loaded =
            domain::ImportEdits(*edits, fingerprint, *definition, base);
        if (!loaded) return std::unexpected(loaded.error());
        current = std::move(*loaded);
        auto changes = domain::Differences(base, current);
        if (!changes.empty())
          signal = editor_.Apply(device_, pools_, sync_, next,
                                 definition->cell_count, changes);
      }
      solver_.reset();
      base_ = std::move(base);
      types_ = std::move(current);
      fingerprint_ = std::move(fingerprint);
      definition_ = *definition;
      min_elevation_ = dem->min_elevation;
      dem_ = std::move(dem);
      lattice_settings_ = settings;
      state_ = SimulationState::kEditing;
      physical_steps_ = 0;
      dam_count_ = 0;
      step_elapsed_ms_ = 0;
      history_.Clear();
      changed_ = types_ != base_;
      lattice_buffer_ = std::move(next);
      snapshot_ = LatticeRenderSnapshot{
          .lattice_buffer = {.native_handle = reinterpret_cast<std::uintptr_t>(
                                 static_cast<VkBuffer>(
                                     *lattice_buffer_.buffer))},
          .lattice_width = definition->width,
          .lattice_height = definition->height,
          .lattice_depth = definition->depth,
          .cell_count = definition->cell_count,
          .terrain_elevation_cells = definition->terrain_elevation_cells,
          .ready_signal = signal};
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not initialize terrain: ") +
                             error.what());
    }
  }
  void Clear() {
    device_.LogicalDevice().waitIdle();
    solver_.reset();
    snapshot_.reset();
    definition_.reset();
    base_.clear();
    types_.clear();
    fingerprint_.clear();
    min_elevation_ = 0;
    history_.Clear();
    changed_ = false;
    dem_.reset();
    state_ = SimulationState::kEditing;
    physical_steps_ = 0;
    dam_count_ = 0;
    step_elapsed_ms_ = 0;
    lattice_buffer_ = {};
  }
  std::expected<void, std::string> Edit(EditOperation const& op) {
    if (!definition_) return std::unexpected("Import a DEM before editing");
    if (state_ != SimulationState::kEditing)
      return std::unexpected("Restore the terrain before editing");
    try {
      auto next = types_;
      auto history = history_;
      if (op.kind == EditOperation::Kind::kUndo ||
          op.kind == EditOperation::Kind::kRedo) {
        history.End(types_);
        auto deltas = op.kind == EditOperation::Kind::kUndo
                          ? history.UndoDeltas()
                          : history.RedoDeltas();
        for (auto delta : deltas)
          next[delta.index] = op.kind == EditOperation::Kind::kUndo
                                  ? delta.before
                                  : delta.after;
      } else if (op.kind == EditOperation::Kind::kRestore)
        next = base_;
      else if (op.kind == EditOperation::Kind::kDam)
        domain::BuildDam(next, *definition_, op.points,
                         op.brush.dam_half_width);
      else if (op.brush.mode == domain::BrushMode::kWater)
        domain::FillBasin(next, *definition_, op.center);
      else
        domain::ApplyBrush(next, *definition_, op.center, op.brush);
      auto changes = domain::Differences(types_, next);
      if (!changes.empty()) {
        if (op.kind == EditOperation::Kind::kBrush ||
            op.kind == EditOperation::Kind::kDam)
          history.Capture(changes);
      }
      if (op.kind == EditOperation::Kind::kUndo) history.CommitUndo();
      if (op.kind == EditOperation::Kind::kRedo) history.CommitRedo();
      if (op.kind == EditOperation::Kind::kRestore) history.Clear();
      // Allocate and stage history before dispatch. Only successful GPU work
      // publishes the corresponding CPU types and history.
      if (!changes.empty()) {
        auto signal = editor_.Apply(device_, pools_, sync_, lattice_buffer_,
                                    definition_->cell_count, changes);
        types_ = std::move(next);
        snapshot_->ready_signal = signal;
        changed_ = types_ != base_;
      }
      history_ = std::move(history);
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(error.what());
    }
  }
  std::expected<void, std::string> Play(domain::LbmSettings const& settings) {
    auto errors = domain::ValidateLbmSettings(settings);
    if (!errors.empty()) return std::unexpected(errors.front());
    if (!snapshot_ || !definition_)
      return std::unexpected("Import a DEM before starting the simulation");
    if (state_ == SimulationState::kRunning) return {};
    if (state_ == SimulationState::kPaused) {
      settings_ = settings;
      state_ = SimulationState::kRunning;
      step_elapsed_ms_ = 1000.0 / settings.steps_per_second;
      return {};
    }
    try {
      history_.End(types_);
      auto solver = std::make_unique<LbmSolver>();
      solver->Initialize(device_, pools_, sync_, lattice_buffer_, *definition_,
                         settings, snapshot_->ready_signal);
      solver_ = std::move(solver);
      dam_count_ = static_cast<std::uint32_t>(
          std::count(types_.begin(), types_.end(),
                     domain::Type(domain::CellType::kObstacleDam)));
      settings_ = settings;
      history_.Clear();
      physical_steps_ = 0;
      step_elapsed_ms_ = 1000.0 / settings.steps_per_second;
      state_ = SimulationState::kRunning;
      snapshot_->ready_signal = sync_.CurrentTimelineValue();
      PublishMomentum();
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not start LBM: ") +
                             error.what());
    }
  }
  void Pause() {
    if (state_ == SimulationState::kRunning) state_ = SimulationState::kPaused;
  }
  std::expected<void, std::string> ResetToTerrain() {
    if (!dem_) return std::unexpected("Import a DEM first");
    return InitializeTerrain(dem_, lattice_settings_, {});
  }
  std::expected<void, std::string> RemoveDam() {
    if (state_ != SimulationState::kRunning)
      return std::unexpected("The dam can only be removed while running");
    if (dam_count_ == 0) return std::unexpected("There is no dam to remove");
    try {
      snapshot_->ready_signal = solver_->RemoveDam(device_, sync_, settings_,
                                                   snapshot_->ready_signal);
      dam_count_ = 0;
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not remove dam: ") +
                             error.what());
    }
  }
  std::optional<LatticeRenderSnapshot> Update(double delta_ms) {
    if (state_ != SimulationState::kRunning || !solver_ || !snapshot_)
      return snapshot_;
    if (std::isfinite(delta_ms) && delta_ms > 0) step_elapsed_ms_ += delta_ms;
    double interval = 1000.0 / settings_.steps_per_second;
    if (step_elapsed_ms_ < interval) return snapshot_;
    try {
      snapshot_->ready_signal = solver_->Step(
          device_, sync_, lattice_buffer_, settings_, snapshot_->ready_signal);
      PublishMomentum();
      ++physical_steps_;
      step_elapsed_ms_ = 0;
    } catch (...) {
      state_ = SimulationState::kPaused;
      throw;
    }
    return snapshot_;
  }
  void PublishMomentum() {
    if (!snapshot_ || !solver_) return;
    auto buffers = solver_->Buffers();
    snapshot_->momentum_buffer.native_handle = reinterpret_cast<std::uintptr_t>(
        static_cast<VkBuffer>(*buffers.momentum[buffers.read_index]->buffer));
    snapshot_->momentum_count = definition_->cell_count * 19U;
  }
  domain::CellTypes base_, types_;
  std::optional<domain::LatticeDefinition> definition_;
  std::string fingerprint_;
  float min_elevation_ = 0;
  bool changed_ = false;
  domain::SharedDem dem_;
  domain::LatticeSettings lattice_settings_;
  domain::LbmSettings settings_;
  SimulationState state_ = SimulationState::kEditing;
  std::uint64_t physical_steps_ = 0;
  std::uint32_t dam_count_ = 0;
  double step_elapsed_ms_ = 0;
  std::unique_ptr<LbmSolver> solver_;
  domain::EditHistory history_;
  LatticeEditor editor_;
  graphics::Device const& device_;
  graphics::CommandPools const& pools_;
  graphics::FrameSync& sync_;
  graphics::AllocatedBuffer lattice_buffer_;
  std::optional<LatticeRenderSnapshot> snapshot_;
};
SimulationSession::SimulationSession(graphics::Device const& device,
                                     graphics::CommandPools const& pools,
                                     graphics::FrameSync& sync)
    : impl_(std::make_unique<Impl>(device, pools, sync)) {}
SimulationSession::~SimulationSession() = default;
std::expected<void, std::string> SimulationSession::InitializeTerrain(
    domain::SharedDem dem, domain::LatticeSettings settings,
    std::optional<domain::LatticeEdits> const& edits) {
  return impl_->InitializeTerrain(std::move(dem), settings, edits);
}
EditState SimulationSession::Editing() const {
  auto types = impl_->state_ == SimulationState::kEditing
                   ? std::span<std::uint8_t const>(impl_->types_)
                   : std::span<std::uint8_t const>{};
  return {impl_->definition_,        types,
          impl_->min_elevation_,     impl_->changed_,
          impl_->history_.CanUndo(), impl_->history_.CanRedo()};
}
void SimulationSession::BeginStroke() {
  if (impl_->state_ == SimulationState::kEditing) impl_->history_.Begin();
}
void SimulationSession::EndStroke() {
  if (impl_->state_ == SimulationState::kEditing)
    impl_->history_.End(impl_->types_);
}
std::expected<void, std::string> SimulationSession::Edit(
    EditOperation const& op) {
  return impl_->Edit(op);
}
std::optional<domain::LatticeEdits> SimulationSession::ExportEdits() const {
  if (!impl_->definition_) return {};
  return domain::ExportEdits(impl_->fingerprint_, *impl_->definition_,
                             impl_->base_, impl_->types_);
}
void SimulationSession::Clear() { impl_->Clear(); }
std::expected<void, std::string> SimulationSession::Play(
    domain::LbmSettings const& settings) {
  return impl_->Play(settings);
}
void SimulationSession::Pause() { impl_->Pause(); }
std::expected<void, std::string> SimulationSession::ResetToTerrain() {
  return impl_->ResetToTerrain();
}
std::expected<void, std::string> SimulationSession::RemoveDam() {
  return impl_->RemoveDam();
}
bool SimulationSession::IsRunning() const {
  return impl_->state_ == SimulationState::kRunning;
}
SimulationState SimulationSession::State() const { return impl_->state_; }
std::uint64_t SimulationSession::PhysicalStepCount() const {
  return impl_->physical_steps_;
}
bool SimulationSession::CanRemoveDam() const {
  return impl_->state_ == SimulationState::kRunning && impl_->dam_count_ > 0;
}
bool SimulationSession::IsReady() const { return impl_->snapshot_.has_value(); }
std::optional<LatticeRenderSnapshot> SimulationSession::Update(
    double delta_ms) {
  return impl_->Update(delta_ms);
}
}  // namespace simulation
