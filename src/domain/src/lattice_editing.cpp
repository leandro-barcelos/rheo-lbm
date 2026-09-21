#include "rheo/domain/lattice_editing.h"

#include "sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
namespace domain {
namespace {
constexpr auto gas = Type(CellType::kGas),
               terrain = Type(CellType::kObstacleTerrain),
               fluid = Type(CellType::kFluid),
               interface = Type(CellType::kInterface),
               dam = Type(CellType::kObstacleDam);
void CheckSize(std::span<std::uint8_t const> types,
               LatticeDefinition const& d) {
  if (!d.width || !d.height || !d.depth || types.size() != d.cell_count ||
      std::uint64_t(d.width) * d.height * d.depth != d.cell_count)
    throw std::invalid_argument("Cell types do not match the lattice");
}
void Raise(CellTypes& t, LatticeDefinition const& d, glm::ivec2 center,
           int radius, int target) {
  target = std::clamp(target, 0, int(d.height) - 1);
  for (auto col : DiscColumns(center, radius, d))
    for (int y = TerrainElevation(t, d, col.x, col.y) + 1; y <= target; ++y)
      t[CellIndex({col.x, y, col.y}, d)] = terrain;
}
}  // namespace
bool Inside(glm::ivec3 p, LatticeDefinition const& d) {
  return p.x >= 0 && p.y >= 0 && p.z >= 0 && unsigned(p.x) < d.width &&
         unsigned(p.y) < d.height && unsigned(p.z) < d.depth;
}
std::uint32_t CellIndex(glm::ivec3 p, LatticeDefinition const& d) {
  return p.x + p.y * d.width + p.z * d.width * d.height;
}
glm::ivec3 CellPosition(std::uint32_t i, LatticeDefinition const& d) {
  return {i % d.width, (i / d.width) % d.height, i / (d.width * d.height)};
}
bool IsObstacle(std::uint8_t t) { return t == terrain || t == dam; }
int TerrainElevation(std::span<std::uint8_t const> t,
                     LatticeDefinition const& d, int x, int z) {
  if (x < 0 || z < 0 || unsigned(x) >= d.width || unsigned(z) >= d.depth)
    return -1;
  for (int y = int(d.height) - 1; y >= 0; --y)
    if (t[CellIndex({x, y, z}, d)] == terrain) return y;
  return -1;
}
std::optional<CellHit> PickCell(Ray const& ray, LatticeDefinition const& d,
                                std::span<std::uint8_t const> t) {
  CheckSize(t, d);
  double scale = std::max({d.width, d.height, d.depth});
  glm::dvec3 shape{d.width, d.height, d.depth};
  glm::dvec3 o = glm::dvec3(ray.origin) * scale + shape * .5,
             v = glm::dvec3(ray.direction) * scale;
  if (!std::isfinite(glm::dot(o, o)) || !std::isfinite(glm::dot(v, v)) ||
      glm::dot(v, v) < 1e-20)
    return {};
  double entry = 0, exit = std::numeric_limits<double>::infinity();
  for (int a = 0; a < 3; ++a) {
    if (std::abs(v[a]) < 1e-12) {
      if (o[a] < 0 || o[a] >= shape[a]) return {};
    } else {
      double first = -o[a] / v[a], last = (shape[a] - o[a]) / v[a];
      if (first > last) std::swap(first, last);
      entry = std::max(entry, first);
      exit = std::min(exit, last);
    }
  }
  if (entry >= exit) return {};
  auto point = glm::clamp(o + v * std::min(exit, entry + 1e-8), glm::dvec3(0),
                          shape - glm::dvec3(1e-7));
  glm::ivec3 p = glm::floor(point), step{};
  glm::dvec3 next{}, delta{};
  for (int a = 0; a < 3; ++a) {
    step[a] = v[a] > 1e-12 ? 1 : v[a] < -1e-12 ? -1 : 0;
    delta[a] =
        step[a] ? std::abs(1 / v[a]) : std::numeric_limits<double>::infinity();
    next[a] =
        step[a] ? ((p[a] + (step[a] > 0 ? 1 : 0)) - o[a]) / v[a] : delta[a];
  }
  glm::ivec3 previous{-1};
  while (Inside(p, d)) {
    if (t[CellIndex(p, d)] != gas) return CellHit{p, previous};
    previous = p;
    double time = std::min({next.x, next.y, next.z});
    if (time > exit) break;
    for (int a = 0; a < 3; ++a)
      if (next[a] <= time + 1e-10) {
        p[a] += step[a];
        next[a] += delta[a];
      }
  }
  return {};
}
std::vector<glm::ivec2> DiscColumns(glm::ivec2 c, int r,
                                    LatticeDefinition const& d) {
  r = std::clamp(r, 0, 32);
  std::vector<glm::ivec2> out;
  for (int z = std::max(0, c.y - r); z <= std::min(int(d.depth) - 1, c.y + r);
       ++z)
    for (int x = std::max(0, c.x - r); x <= std::min(int(d.width) - 1, c.x + r);
         ++x)
      if ((x - c.x) * (x - c.x) + (z - c.y) * (z - c.y) <= r * r)
        out.emplace_back(x, z);
  return out;
}
std::vector<glm::ivec2> DamColumns(std::span<glm::ivec3 const> points,
                                   int radius, LatticeDefinition const& d) {
  std::set<std::pair<int, int>> unique;
  for (auto p : points)
    if (!Inside(p, d))
      throw std::invalid_argument("Dam point is outside the lattice");
  for (std::size_t i = 1; i < points.size(); ++i) {
    int x = points[i - 1].x, z = points[i - 1].z, tx = points[i].x,
        tz = points[i].z;
    int dx = std::abs(tx - x), sx = x < tx ? 1 : -1, dz = -std::abs(tz - z),
        sz = z < tz ? 1 : -1, error = dx + dz;
    for (;;) {
      for (auto c : DiscColumns({x, z}, radius, d)) unique.emplace(c.x, c.y);
      if (x == tx && z == tz) break;
      int twice = 2 * error;
      if (twice >= dz) {
        error += dz;
        x += sx;
      }
      if (twice <= dx) {
        error += dx;
        z += sz;
      }
    }
  }
  std::vector<glm::ivec2> out;
  for (auto [x, z] : unique) out.emplace_back(x, z);
  return out;
}
void RebuildInterfaces(CellTypes& t, LatticeDefinition const& d) {
  CheckSize(t, d);
  for (unsigned z = 0; z < d.depth; ++z)
    for (unsigned x = 0; x < d.width; ++x) t[CellIndex({x, 0, z}, d)] = terrain;
  for (auto& type : t)
    if (type == interface) type = fluid;
  for (unsigned i = 0; i < t.size(); ++i)
    if (t[i] == fluid) {
      auto p = CellPosition(i, d);
      bool adjacent = false;
      for (int z = -1; z <= 1; ++z)
        for (int y = -1; y <= 1; ++y)
          for (int x = -1; x <= 1; ++x) {
            int length = std::abs(x) + std::abs(y) + std::abs(z);
            if (length < 1 || length > 2) continue;
            auto n = p + glm::ivec3(x, y, z);
            if (Inside(n, d) && t[CellIndex(n, d)] == gas) adjacent = true;
          }
      if (adjacent) t[i] = interface;
    }
}
void ApplyBrush(CellTypes& t, LatticeDefinition const& d, glm::ivec3 center,
                BrushSettings const& brush) {
  CheckSize(t, d);
  if (!Inside(center, d)) return;
  int radius = std::clamp(brush.radius, 1, 32);
  if (brush.mode == BrushMode::kElevation || brush.mode == BrushMode::kTerrain)
    Raise(t, d, {center.x, center.z}, radius,
          brush.mode == BrushMode::kTerrain ? center.y : brush.elevation);
  else if (brush.mode == BrushMode::kErase && center.y > 0) {
    for (auto col : DiscColumns({center.x, center.z}, radius, d)) {
      auto i = CellIndex({col.x, center.y, col.y}, d);
      bool exposed = t[i] != gas;
      for (int y = center.y + 1; y < int(d.height); ++y)
        if (t[CellIndex({col.x, y, col.y}, d)] != gas) {
          exposed = false;
          break;
        }
      if (exposed) t[i] = gas;
    }
  }
  RebuildInterfaces(t, d);
}
void BuildDam(CellTypes& t, LatticeDefinition const& d,
              std::span<glm::ivec3 const> points, int radius) {
  CheckSize(t, d);
  if (points.size() < 2) return;
  int top = std::min(points.front().y, points.back().y);
  for (auto col : DamColumns(points, std::clamp(radius, 0, 16), d))
    for (int y = TerrainElevation(t, d, col.x, col.y) + 1; y <= top; ++y)
      t[CellIndex({col.x, y, col.y}, d)] = dam;
  RebuildInterfaces(t, d);
}
int FillBasin(CellTypes& t, LatticeDefinition const& d, glm::ivec3 seed) {
  CheckSize(t, d);
  int highest = seed.y - 1;
  if (!Inside(seed, d) || t[CellIndex(seed, d)] != gas) return highest;
  for (int y = seed.y; y < int(d.height) - 1; ++y) {
    if (IsObstacle(t[CellIndex({seed.x, y, seed.z}, d)])) break;
    std::vector<bool> visited(std::size_t(d.width) * d.depth);
    std::vector<glm::ivec2> queue{{seed.x, seed.z}};
    visited[seed.x + seed.z * d.width] = true;
    bool spills = false;
    for (std::size_t q = 0; q < queue.size(); ++q) {
      auto c = queue[q];
      if (c.x == 0 || c.y == 0 || c.x == int(d.width) - 1 ||
          c.y == int(d.depth) - 1)
        spills = true;
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
          if (!dx && !dz) continue;
          auto n = c + glm::ivec2(dx, dz);
          glm::ivec3 p{n.x, y, n.y};
          if (!Inside(p, d) || visited[n.x + n.y * d.width] ||
              IsObstacle(t[CellIndex(p, d)]))
            continue;
          if (dx && dz &&
              (IsObstacle(t[CellIndex({n.x, y, c.y}, d)]) ||
               IsObstacle(t[CellIndex({c.x, y, n.y}, d)])))
            continue;
          visited[n.x + n.y * d.width] = true;
          queue.push_back(n);
        }
    }
    if (spills) break;
    for (auto c : queue)
      if (t[CellIndex({c.x, y, c.y}, d)] == gas)
        t[CellIndex({c.x, y, c.y}, d)] = fluid;
    highest = y;
  }
  RebuildInterfaces(t, d);
  return highest;
}
std::vector<CellDelta> Differences(std::span<std::uint8_t const> before,
                                   std::span<std::uint8_t const> after) {
  if (before.size() != after.size())
    throw std::invalid_argument("Mismatched cell arrays");
  std::vector<CellDelta> out;
  for (std::size_t i = 0; i < after.size(); ++i)
    if (before[i] != after[i])
      out.push_back({std::uint32_t(i), before[i], after[i]});
  return out;
}
std::string DemFingerprint(DemData const& dem) {
  detail::Sha256 hash;
  auto word = [&](std::uint32_t value) {
    std::array<unsigned char, 4> bytes{static_cast<unsigned char>(value),
                                       static_cast<unsigned char>(value >> 8),
                                       static_cast<unsigned char>(value >> 16),
                                       static_cast<unsigned char>(value >> 24)};
    hash.Update(bytes);
  };
  word(dem.width);
  word(dem.height);
  for (float v : {dem.pixel_size_meters.x, dem.pixel_size_meters.y,
                  dem.min_elevation, dem.max_elevation})
    word(std::bit_cast<std::uint32_t>(v));
  for (auto const& sample : dem.samples)
    word(std::bit_cast<std::uint32_t>(sample.elevation));
  auto digest = hash.Finish();
  std::string out;
  for (auto b : digest) {
    out += '0';
    out.back() = "0123456789abcdef"[b >> 4];
    out += "0123456789abcdef"[b & 15];
  }
  return out;
}
LatticeEdits ExportEdits(std::string fingerprint, LatticeDefinition const& d,
                         CellTypes const& base, CellTypes const& current) {
  LatticeEdits out{std::move(fingerprint), d, {}};
  for (auto delta : Differences(base, current)) {
    if (!out.runs.empty() &&
        out.runs.back().start + out.runs.back().count == delta.index &&
        out.runs.back().type == delta.after)
      ++out.runs.back().count;
    else
      out.runs.push_back({delta.index, 1, delta.after});
  }
  return out;
}
std::expected<CellTypes, std::string> ImportEdits(
    LatticeEdits const& e, std::string const& fingerprint,
    LatticeDefinition const& d, CellTypes const& base) {
  if (e.dem_fingerprint != fingerprint || e.definition.width != d.width ||
      e.definition.height != d.height || e.definition.depth != d.depth ||
      e.definition.cell_count != d.cell_count ||
      e.definition.meters_per_cell != d.meters_per_cell ||
      e.definition.terrain_elevation_cells != d.terrain_elevation_cells)
    return std::unexpected(
        "Edits are incompatible with this DEM or lattice definition");
  CellTypes result = base;
  std::uint64_t end = 0;
  for (auto run : e.runs) {
    auto next = std::uint64_t(run.start) + run.count;
    if (!run.count || run.start < end || next > d.cell_count || run.type > dam)
      return std::unexpected("Invalid or overlapping lattice edit range");
    for (std::uint64_t i = run.start; i < next; ++i) {
      if ((i / d.width) % d.height == 0 && run.type != terrain)
        return std::unexpected("Edits cannot remove the terrain base");
      result[i] = run.type;
    }
    end = next;
  }
  auto normalized = result;
  RebuildInterfaces(normalized, d);
  if (normalized != result)
    return std::unexpected("Saved fluid interfaces are inconsistent");
  return result;
}
void EditHistory::Begin() {
  if (!recording_) {
    recording_ = true;
    before_.clear();
  }
}
void EditHistory::Capture(std::span<CellDelta const> deltas) {
  if (recording_)
    for (auto d : deltas) before_.try_emplace(d.index, d.before);
}
void EditHistory::End(CellTypes const& types) {
  if (!recording_) return;
  recording_ = false;
  std::vector<CellDelta> op;
  op.reserve(before_.size());
  for (auto [i, t] : before_)
    if (t != types[i]) op.push_back({i, t, types[i]});
  before_.clear();
  if (op.empty()) return;
  std::ranges::sort(op, {}, &CellDelta::index);
  redo_.clear();
  undo_.push_back(
      std::make_shared<std::vector<CellDelta> const>(std::move(op)));
  auto bytes = [&] {
    std::size_t n = 0;
    for (auto const& o : undo_)
      n += sizeof(o) + sizeof(*o) + o->capacity() * sizeof(CellDelta);
    return n;
  };
  while (!undo_.empty() &&
         (undo_.size() > max_operations_ || bytes() > max_bytes_))
    undo_.erase(undo_.begin());
}
void EditHistory::Clear() {
  recording_ = false;
  before_.clear();
  undo_.clear();
  redo_.clear();
}
std::span<CellDelta const> EditHistory::UndoDeltas() const {
  return CanUndo() ? std::span<CellDelta const>(*undo_.back())
                   : std::span<CellDelta const>{};
}
std::span<CellDelta const> EditHistory::RedoDeltas() const {
  return CanRedo() ? std::span<CellDelta const>(*redo_.back())
                   : std::span<CellDelta const>{};
}
void EditHistory::CommitUndo() {
  if (CanUndo()) {
    redo_.push_back(std::move(undo_.back()));
    undo_.pop_back();
  }
}
void EditHistory::CommitRedo() {
  if (CanRedo()) {
    undo_.push_back(std::move(redo_.back()));
    redo_.pop_back();
  }
}
}  // namespace domain
