#ifndef RHEO_DOMAIN_LATTICE_EDITING_H
#define RHEO_DOMAIN_LATTICE_EDITING_H
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "rheo/domain/lattice_definition.h"
namespace domain {
enum class CellType : std::int32_t {
  kFluid = 0,
  kInterface = 1,
  kGas = 2,
  kObstacleTerrain = 3,
  kObstacleDam = 4,
  kInterfaceToFluid = 5,
  kInterfaceToGas = 6
};
using CellTypes = std::vector<std::uint8_t>;
constexpr std::uint8_t Type(CellType t) { return static_cast<std::uint8_t>(t); }
constexpr bool IsPersistentCellType(std::uint8_t type) {
  return type <= Type(CellType::kObstacleDam);
}
struct Ray {
  glm::vec3 origin{}, direction{};
};
struct CellHit {
  glm::ivec3 surface{}, previous_gas{-1};
};
enum class BrushMode {
  kErase,
  kTerrain,
  kElevation,
  kSampleElevation,
  kDam,
  kWater
};
struct BrushSettings {
  BrushMode mode = BrushMode::kErase;
  int radius = 2, dam_half_width = 0, elevation = 1;
  bool operator==(BrushSettings const&) const = default;
};
struct BrushPreview {
  bool active = false;
  glm::ivec3 center{};
  BrushSettings settings;
  std::vector<glm::ivec3> dam_points;
};
struct CellDelta {
  std::uint32_t index;
  std::uint8_t before, after;
};
struct EditRun {
  std::uint32_t start, count;
  std::uint8_t type;
  bool operator==(EditRun const&) const = default;
};
struct LatticeEdits {
  std::string dem_fingerprint;
  LatticeDefinition definition{};
  std::vector<EditRun> runs;
};
[[nodiscard]] std::string DemFingerprint(DemData const& dem);
[[nodiscard]] bool Inside(glm::ivec3 p, LatticeDefinition const& d);
[[nodiscard]] std::uint32_t CellIndex(glm::ivec3 p, LatticeDefinition const& d);
[[nodiscard]] glm::ivec3 CellPosition(std::uint32_t index,
                                      LatticeDefinition const& d);
[[nodiscard]] bool IsObstacle(std::uint8_t type);
[[nodiscard]] int TerrainElevation(std::span<std::uint8_t const> types,
                                   LatticeDefinition const& d, int x, int z);
std::optional<CellHit> PickCell(Ray const& world_ray,
                                LatticeDefinition const& d,
                                std::span<std::uint8_t const> types);
std::vector<glm::ivec2> DiscColumns(glm::ivec2 center, int radius,
                                    LatticeDefinition const& d);
std::vector<glm::ivec2> DamColumns(std::span<glm::ivec3 const> points,
                                   int radius, LatticeDefinition const& d);
void RebuildInterfaces(CellTypes& types, LatticeDefinition const& d);
void ApplyBrush(CellTypes& types, LatticeDefinition const& d, glm::ivec3 center,
                BrushSettings const& brush);
void BuildDam(CellTypes& types, LatticeDefinition const& d,
              std::span<glm::ivec3 const> points, int radius);
int FillBasin(CellTypes& types, LatticeDefinition const& d, glm::ivec3 seed);
std::vector<CellDelta> Differences(std::span<std::uint8_t const> before,
                                   std::span<std::uint8_t const> after);
LatticeEdits ExportEdits(std::string fingerprint, LatticeDefinition const& d,
                         CellTypes const& base, CellTypes const& current);
std::expected<CellTypes, std::string> ImportEdits(
    LatticeEdits const& edits, std::string const& fingerprint,
    LatticeDefinition const& d, CellTypes const& base);
class EditHistory {
 public:
  explicit EditHistory(std::size_t max_operations = 100,
                       std::size_t max_bytes = 64 * 1024 * 1024)
      : max_operations_(max_operations), max_bytes_(max_bytes) {}
  void Begin();
  void Capture(std::span<CellDelta const> deltas);
  void End(CellTypes const& types);
  void Clear();
  bool Recording() const { return recording_; }
  bool CanUndo() const { return !recording_ && !undo_.empty(); }
  bool CanRedo() const { return !recording_ && !redo_.empty(); }
  std::span<CellDelta const> UndoDeltas() const;
  std::span<CellDelta const> RedoDeltas() const;
  void CommitUndo();
  void CommitRedo();

 private:
  std::size_t max_operations_, max_bytes_;
  bool recording_ = false;
  std::unordered_map<std::uint32_t, std::uint8_t> before_;
  // Completed operations are immutable; transaction staging shares their
  // storage.
  std::vector<std::shared_ptr<std::vector<CellDelta> const>> undo_, redo_;
};
}  // namespace domain
#endif
