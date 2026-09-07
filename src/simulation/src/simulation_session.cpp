#include "rheo/simulation/simulation_session.h"

#include <exception>
#include <utility>

#include "lattice_editor.h"
#include "lattice_initializer.h"
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
      base_ = std::move(base);
      types_ = std::move(current);
      fingerprint_ = std::move(fingerprint);
      definition_ = *definition;
      min_elevation_ = dem->min_elevation;
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
    snapshot_.reset();
    definition_.reset();
    base_.clear();
    types_.clear();
    fingerprint_.clear();
    min_elevation_ = 0;
    history_.Clear();
    changed_ = false;
    lattice_buffer_ = {};
  }
  std::expected<void, std::string> Edit(EditOperation const& op) {
    if (!definition_) return std::unexpected("Import a DEM before editing");
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
  domain::CellTypes base_, types_;
  std::optional<domain::LatticeDefinition> definition_;
  std::string fingerprint_;
  float min_elevation_ = 0;
  bool changed_ = false;
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
  return {impl_->definition_,        impl_->types_,
          impl_->min_elevation_,     impl_->changed_,
          impl_->history_.CanUndo(), impl_->history_.CanRedo()};
}
void SimulationSession::BeginStroke() { impl_->history_.Begin(); }
void SimulationSession::EndStroke() { impl_->history_.End(impl_->types_); }
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
void SimulationSession::Play() {
}  // Fluid dynamics will be implemented in the next stage.
void SimulationSession::Pause() {}
bool SimulationSession::IsRunning() const { return false; }
bool SimulationSession::IsReady() const { return impl_->snapshot_.has_value(); }
std::optional<LatticeRenderSnapshot> SimulationSession::Update(double) {
  return impl_->snapshot_;
}
}  // namespace simulation
