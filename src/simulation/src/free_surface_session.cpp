#include "rheo/simulation/free_surface_session.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

#include "free_surface_solver.h"
#include "lattice_editor.h"
#include "lattice_initializer.h"

namespace simulation {

class FreeSurfaceSession::Impl {
 public:
  Impl(graphics::Device const& device, graphics::CommandPools const& pools,
       graphics::FrameSync& sync)
      : device(device), pools(pools), sync(sync) {}
  std::expected<void, std::string> InitializeTerrain(
      domain::SharedDem dem, domain::LatticeSettings settings,
      std::optional<domain::LatticeEdits> const& edits) {
    if (!dem) {
      return std::unexpected("Import a valid DEM first");
    }

    auto definition = domain::DefineLattice(*dem, settings);
    if (!definition) {
      return std::unexpected(definition.error());
    }

    try {
      std::uint64_t signal = 0;
      // Older frames must finish before the shared timeline advances on
      // compute.
      device.LogicalDevice().waitIdle();
      domain::CellTypes base;
      auto next = LatticeInitializer::Build(device, pools, sync, *dem,
                                            *definition, signal, base);
      auto fingerprint = domain::DemFingerprint(*dem);
      auto current = base;
      if (edits) {
        auto loaded =
            domain::ImportEdits(*edits, fingerprint, *definition, base);
        if (!loaded) {
          return std::unexpected(loaded.error());
        }

        current = std::move(*loaded);
        auto changes = domain::Differences(base, current);
        if (!changes.empty()) {
          signal = editor.Apply(device, pools, sync, next,
                                definition->cell_count, changes);
        }
      }
      solver.reset();
      this->base = std::move(base);
      types = std::move(current);
      this->fingerprint = std::move(fingerprint);
      this->definition = *definition;
      min_elevation = dem->min_elevation;
      this->dem = std::move(dem);
      lattice_settings = settings;
      state = SimulationState::kEditing;
      physical_steps = 0;
      dam_count = 0;
      step_elapsed_ms = 0;
      history.Clear();
      changed = types != this->base;
      lattice_buffer = std::move(next);
      snapshot = LatticeRenderSnapshot{
          .lattice_buffer = {.native_handle = reinterpret_cast<std::uintptr_t>(
                                 static_cast<VkBuffer>(
                                     *lattice_buffer.Buffer()))},
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
    device.LogicalDevice().waitIdle();
    solver.reset();
    snapshot.reset();
    definition.reset();
    base.clear();
    types.clear();
    fingerprint.clear();
    min_elevation = 0;
    history.Clear();
    changed = false;
    dem.reset();
    state = SimulationState::kEditing;
    physical_steps = 0;
    dam_count = 0;
    step_elapsed_ms = 0;
    lattice_buffer = {};
  }

  std::expected<void, std::string> Edit(EditOperation const& operation) {
    if (!definition) {
      return std::unexpected("Import a DEM before editing");
    }

    if (state != SimulationState::kEditing) {
      return std::unexpected("Restore the terrain before editing");
    }

    try {
      auto next = types;
      auto history = this->history;
      if (operation.kind == EditOperation::Kind::kUndo ||
          operation.kind == EditOperation::Kind::kRedo) {
        history.End(types);
        auto deltas = operation.kind == EditOperation::Kind::kUndo
                          ? history.UndoDeltas()
                          : history.RedoDeltas();

        for (auto delta : deltas) {
          next[delta.index] = operation.kind == EditOperation::Kind::kUndo
                                  ? delta.before
                                  : delta.after;
        }
      } else if (operation.kind == EditOperation::Kind::kRestore) {
        next = base;
      } else if (operation.kind == EditOperation::Kind::kDam) {
        domain::BuildDam(next, *definition, operation.points,
                         operation.brush.dam_half_width);
      } else if (operation.brush.mode == domain::BrushMode::kWater) {
        domain::FillBasin(next, *definition, operation.center);
      } else {
        domain::ApplyBrush(next, *definition, operation.center,
                           operation.brush);
      }

      auto changes = domain::Differences(types, next);
      if (!changes.empty()) {
        if (operation.kind == EditOperation::Kind::kBrush ||
            operation.kind == EditOperation::Kind::kDam) {
          history.Capture(changes);
        }
      }

      if (operation.kind == EditOperation::Kind::kUndo) {
        history.CommitUndo();
      }

      if (operation.kind == EditOperation::Kind::kRedo) {
        history.CommitRedo();
      }

      if (operation.kind == EditOperation::Kind::kRestore) {
        history.Clear();
      }

      // Allocate and stage history before dispatch. Only successful GPU work
      // publishes the corresponding CPU types and history.
      if (!changes.empty()) {
        auto signal = editor.Apply(device, pools, sync, lattice_buffer,
                                   definition->cell_count, changes);
        types = std::move(next);
        snapshot->ready_signal = signal;
        changed = types != base;
      }

      this->history = std::move(history);
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(error.what());
    }
  }

  std::expected<void, std::string> Play(domain::LbmSettings const& settings) {
    auto errors = domain::ValidateLbmSettings(settings);
    if (!errors.empty()) {
      return std::unexpected(errors.front());
    }

    if (!snapshot || !definition) {
      return std::unexpected("Import a DEM before starting the simulation");
    }

    if (state == SimulationState::kRunning) {
      return {};
    }

    if (state == SimulationState::kPaused) {
      this->settings = settings;
      state = SimulationState::kRunning;
      step_elapsed_ms = 1000.0 / settings.steps_per_second;
      return {};
    }

    try {
      history.End(types);
      auto solver = std::make_unique<FreeSurfaceSolver>();
      solver->Initialize(device, pools, sync, lattice_buffer, *definition,
                         settings, snapshot->ready_signal);
      this->solver = std::move(solver);
      dam_count = static_cast<std::uint32_t>(std::ranges::count(
          types, domain::Type(domain::CellType::kObstacleDam)));
      this->settings = settings;
      history.Clear();
      physical_steps = 0;
      step_elapsed_ms = 1000.0 / settings.steps_per_second;
      state = SimulationState::kRunning;
      snapshot->ready_signal = sync.CurrentTimelineValue();
      PublishMomentum();
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not start LBM: ") +
                             error.what());
    }
  }

  void Pause() {
    if (state == SimulationState::kRunning) {
      state = SimulationState::kPaused;
    }
  }

  std::expected<void, std::string> ResetToTerrain() {
    if (!dem) {
      return std::unexpected("Import a DEM first");
    }

    return InitializeTerrain(dem, lattice_settings, {});
  }

  std::expected<void, std::string> RemoveDam() {
    if (state != SimulationState::kRunning) {
      return std::unexpected("The dam can only be removed while running");
    }

    if (dam_count == 0) {
      return std::unexpected("There is no dam to remove");
    }

    try {
      snapshot->ready_signal =
          solver->RemoveDam(device, sync, settings, snapshot->ready_signal);
      dam_count = 0;
      return {};
    } catch (std::exception const& error) {
      return std::unexpected(std::string("Could not remove dam: ") +
                             error.what());
    }
  }

  std::optional<LatticeRenderSnapshot> Update(double delta_ms) {
    if (state != SimulationState::kRunning || !solver || !snapshot) {
      return snapshot;
    }

    if (std::isfinite(delta_ms) && delta_ms > 0) {
      step_elapsed_ms += delta_ms;
    }

    double interval = 1000.0 / settings.steps_per_second;
    if (step_elapsed_ms < interval) {
      return snapshot;
    }

    try {
      snapshot->ready_signal = solver->Step(device, sync, lattice_buffer,
                                            settings, snapshot->ready_signal);
      PublishMomentum();
      ++physical_steps;
      step_elapsed_ms = 0;
    } catch (...) {
      state = SimulationState::kPaused;
      throw;
    }

    return snapshot;
  }
  void PublishMomentum() {
    if (!snapshot || !solver) {
      return;
    }

    auto buffers = solver->Buffers();
    snapshot->momentum_buffer.native_handle =
        reinterpret_cast<std::uintptr_t>(static_cast<VkBuffer>(
            *buffers.momentum.at(buffers.read_index)->Buffer()));
    snapshot->momentum_count = definition->cell_count * 19U;
  }

  domain::CellTypes base, types;
  std::optional<domain::LatticeDefinition> definition;
  std::string fingerprint;
  float min_elevation = 0;
  bool changed = false;
  domain::SharedDem dem;
  domain::LatticeSettings lattice_settings;
  domain::LbmSettings settings;
  SimulationState state = SimulationState::kEditing;
  std::uint64_t physical_steps = 0;
  std::uint32_t dam_count = 0;
  double step_elapsed_ms = 0;
  std::unique_ptr<FreeSurfaceSolver> solver;
  domain::EditHistory history;
  LatticeEditor editor;
  graphics::Device const& device;
  graphics::CommandPools const& pools;
  graphics::FrameSync& sync;
  graphics::AllocatedBuffer lattice_buffer;
  std::optional<LatticeRenderSnapshot> snapshot;
};

FreeSurfaceSession::FreeSurfaceSession(graphics::Device const& device,
                                       graphics::CommandPools const& pools,
                                       graphics::FrameSync& sync)
    : impl_(std::make_unique<Impl>(device, pools, sync)) {}
FreeSurfaceSession::~FreeSurfaceSession() = default;
std::expected<void, std::string> FreeSurfaceSession::InitializeTerrain(
    domain::SharedDem dem, domain::LatticeSettings settings,
    std::optional<domain::LatticeEdits> const& edits) {
  return impl_->InitializeTerrain(std::move(dem), settings, edits);
}

EditState FreeSurfaceSession::Editing() const {
  auto types = impl_->state == SimulationState::kEditing
                   ? std::span<std::uint8_t const>(impl_->types)
                   : std::span<std::uint8_t const>{};
  return {.definition = impl_->definition,
          .types = types,
          .min_elevation = impl_->min_elevation,
          .changed = impl_->changed,
          .can_undo = impl_->history.CanUndo(),
          .can_redo = impl_->history.CanRedo()};
}

std::optional<domain::CellHit> FreeSurfaceSession::Select(
    domain::Ray const& ray) const {
  auto state = Editing();
  return state.definition
             ? domain::PickCell(ray, *state.definition, state.types)
             : std::nullopt;
}

void FreeSurfaceSession::BeginStroke() {
  if (impl_->state == SimulationState::kEditing) {
    impl_->history.Begin();
  }
}

void FreeSurfaceSession::EndStroke() {
  if (impl_->state == SimulationState::kEditing) {
    impl_->history.End(impl_->types);
  }
}

std::expected<void, std::string> FreeSurfaceSession::Edit(
    EditOperation const& operation) {
  return impl_->Edit(operation);
}

std::optional<domain::LatticeEdits> FreeSurfaceSession::ExportEdits() const {
  if (!impl_->definition) {
    return {};
  }
  return domain::ExportEdits(impl_->fingerprint, *impl_->definition,
                             impl_->base, impl_->types);
}

void FreeSurfaceSession::Clear() { impl_->Clear(); }
std::expected<void, std::string> FreeSurfaceSession::Play(
    domain::LbmSettings const& settings) {
  return impl_->Play(settings);
}

void FreeSurfaceSession::Pause() { impl_->Pause(); }
std::expected<void, std::string> FreeSurfaceSession::ResetToTerrain() {
  return impl_->ResetToTerrain();
}

std::expected<void, std::string> FreeSurfaceSession::RemoveDam() {
  return impl_->RemoveDam();
}

bool FreeSurfaceSession::IsRunning() const {
  return impl_->state == SimulationState::kRunning;
}

SimulationState FreeSurfaceSession::State() const { return impl_->state; }
std::uint64_t FreeSurfaceSession::PhysicalStepCount() const {
  return impl_->physical_steps;
}

bool FreeSurfaceSession::CanRemoveDam() const {
  return impl_->state == SimulationState::kRunning && impl_->dam_count > 0;
}

bool FreeSurfaceSession::IsReady() const { return impl_->snapshot.has_value(); }

std::optional<LatticeRenderSnapshot> FreeSurfaceSession::Update(
    double delta_ms) {
  return impl_->Update(delta_ms);
}

}  // namespace simulation
