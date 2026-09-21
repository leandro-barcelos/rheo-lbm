#include "rheo/domain/lattice_editing.h"

#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace domain;
void Check(bool v, std::source_location l = std::source_location::current()) {
  if (!v)
    throw std::runtime_error("Editing check failed at line " +
                             std::to_string(l.line()));
}
constexpr auto gas = Type(CellType::kGas),
               terrain = Type(CellType::kObstacleTerrain),
               dam = Type(CellType::kObstacleDam),
               fluid = Type(CellType::kFluid),
               surface = Type(CellType::kInterface);
LatticeDefinition const d{7, 6, 7, 294, 2, 5};
CellTypes Base() {
  CellTypes t(d.cell_count, gas);
  RebuildInterfaces(t, d);
  return t;
}
void Set(CellTypes& t, glm::ivec3 p, std::uint8_t type) {
  t[CellIndex(p, d)] = type;
}
auto Get(CellTypes const& t, glm::ivec3 p) { return t[CellIndex(p, d)]; }
int main() {
  // Preserve the serialized DEM hash used in existing project files.
  DemData dem;
  dem.width = 2;
  dem.height = 2;
  dem.pixel_size_meters = {1.5F, 2.5F};
  dem.min_elevation = -2;
  dem.max_elevation = 7;
  for (float elevation : {-2.0F, 0.0F, 3.5F, 7.0F}) {
    Elevation sample{};
    sample.elevation = elevation;
    dem.samples.push_back(sample);
  }
  Check(DemFingerprint(dem) == "2cf21db61f7bc128cbdd28b5f405f3bcd1cc61865caa9451f27a37412885d72c");

  auto t = Base(), base = t;
  auto ray = [](float x, float z) {
    return Ray{{(x + .5F) / 7 - .5F, 2, (z + .5F) / 7 - .5F}, {0, -1, 0}};
  };
  for (int z : {0, 3, 6})
    for (int x : {0, 3, 6}) {
      auto h = PickCell(ray(x, z), d, t);
      Check(h && h->surface == glm::ivec3(x, 0, z) &&
            h->previous_gas == glm::ivec3(x, 1, z));
    }
  Check(!PickCell({{2, 2, 0}, {0, -1, 0}}, d, t));
  Check(!PickCell({{0, 2, 0}, {1, 0, 0}}, d, t));
  Check(!PickCell({{0, 0, 0}, {0, 0, 0}}, d, t));
  auto h = PickCell({{-.5F, 2, -.5F}, {0, -1, 0}}, d, t);
  Check(h && h->surface == glm::ivec3(0));
  h = PickCell({{0, -2, 0}, {0, 1, 0}}, d, t);
  Check(h && h->previous_gas == glm::ivec3(-1));
  ApplyBrush(t, d, {3, 0, 3}, {.mode = BrushMode::kErase});
  Check(t == base);
  ApplyBrush(t, d, {3, 1, 3}, {.mode = BrushMode::kTerrain, .radius = 1});
  Check(Get(t, {3, 1, 3}) == terrain && Get(t, {4, 1, 4}) == gas);
  Set(t, {3, 3, 3}, dam);
  ApplyBrush(t, d, {3, 1, 3}, {.mode = BrushMode::kErase, .radius = 1});
  Check(Get(t, {3, 1, 3}) == terrain && Get(t, {4, 1, 3}) == gas);
  ApplyBrush(t, d, {3, 1, 3},
             {.mode = BrushMode::kElevation, .radius = 1, .elevation = 2});
  Check(Get(t, {3, 2, 3}) == terrain && Get(t, {3, 3, 3}) == dam);
  ApplyBrush(t, d, {3, 1, 3},
             {.mode = BrushMode::kElevation, .radius = 1, .elevation = 1});
  Check(Get(t, {3, 2, 3}) == terrain);
  Check(TerrainElevation(t, d, 3, 3) == 2);
  t = base;
  Set(t, {1, 4, 3}, terrain);
  Set(t, {5, 2, 3}, terrain);
  std::vector<glm::ivec3> points{{1, 4, 3}, {3, 5, 3}, {5, 2, 3}};
  BuildDam(t, d, points, 0);
  Check(Get(t, {3, 1, 3}) == dam && Get(t, {3, 2, 3}) == dam &&
        Get(t, {3, 3, 3}) == gas && Get(t, {1, 4, 3}) == terrain);
  auto columns = DamColumns(points, 1, d);
  Check(columns.size() == 17);
  t = base;
  Check(FillBasin(t, d, {3, 1, 3}) == 0 && t == base);
  for (int z = 1; z <= 5; ++z)
    for (int x = 1; x <= 5; ++x)
      if (x == 1 || x == 5 || z == 1 || z == 5)
        for (int y = 1; y <= 3; ++y) Set(t, {x, y, z}, dam);
  Check(FillBasin(t, d, {3, 1, 3}) == 3);
  Check(Get(t, {3, 1, 3}) == fluid && Get(t, {3, 3, 3}) == surface &&
        Get(t, {3, 4, 3}) == gas && Get(t, {3, 5, 3}) == gas);
  // Corner opening cannot drain through two blocking orthogonal neighbors.
  t = base;
  for (auto p : {glm::ivec3(2, 1, 3), glm::ivec3(4, 1, 3), glm::ivec3(3, 1, 2),
                 glm::ivec3(3, 1, 4)})
    Set(t, p, dam);
  Check(FillBasin(t, d, {3, 1, 3}) == 1 && Get(t, {3, 1, 3}) == surface);
  t = base;
  for (auto& type : t) type = fluid;
  RebuildInterfaces(t, d);
  Set(t, {4, 3, 4}, gas);
  RebuildInterfaces(t, d);
  Check(Get(t, {3, 3, 3}) == surface);  // face diagonal belongs to D3Q19
  t = base;
  for (auto& type : t) type = fluid;
  Set(t, {4, 4, 4}, gas);
  RebuildInterfaces(t, d);
  Check(Get(t, {3, 3, 3}) == fluid);  // body diagonal does not
  auto edits = ExportEdits("hash", d, base, t);
  auto imported = ImportEdits(edits, "hash", d, base);
  Check(imported && *imported == t);
  Check(!ImportEdits(edits, "other", d, base));
  edits.runs = {{0, 1, gas}};
  Check(!ImportEdits(edits, "hash", d, base));
  edits.runs = {{8, 2, terrain}, {9, 1, terrain}};
  Check(!ImportEdits(edits, "hash", d, base));
  edits.runs = {{d.cell_count, 1, terrain}};
  Check(!ImportEdits(edits, "hash", d, base));
  edits.runs = {{CellIndex({3, 2, 3}, d), 1, fluid}};
  Check(!ImportEdits(edits, "hash", d, base));
  EditHistory history(2, 1024);
  t = base;
  history.Begin();
  auto n = t;
  Set(n, {3, 1, 3}, terrain);
  history.Capture(Differences(t, n));
  t = n;
  n = t;
  Set(n, {3, 1, 3}, dam);
  Set(n, {4, 1, 3}, terrain);
  history.Capture(Differences(t, n));
  t = n;
  history.End(t);
  Check(history.CanUndo() && history.UndoDeltas().size() == 2 &&
        history.UndoDeltas()[0].before == gas);
  for (auto delta : history.UndoDeltas()) t[delta.index] = delta.before;
  history.CommitUndo();
  Check(t == base && history.CanRedo());
  history.Begin();
  history.End(t);
  Check(history.CanRedo());
  for (auto delta : history.RedoDeltas()) t[delta.index] = delta.after;
  history.CommitRedo();
  Check(history.CanUndo());
  for (int x = 0; x < 4; ++x) {
    history.Begin();
    n = t;
    Set(n, {x, 2, 3}, terrain);
    history.Capture(Differences(t, n));
    t = n;
    history.End(t);
  }
  int undos = 0;
  while (history.CanUndo()) {
    history.CommitUndo();
    ++undos;
  }
  Check(undos == 2);
  history.Begin();
  n = t;
  Set(n, {0, 3, 3}, terrain);
  history.Capture(Differences(t, n));
  t = n;
  history.End(t);
  Check(!history.CanRedo());
  EditHistory tiny(100, 1);
  tiny.Begin();
  tiny.Capture(Differences(base, t));
  tiny.End(t);
  Check(!tiny.CanUndo());
  std::cout << "Lattice tools, DDA, D3Q19, persistence and history passed\n";
}
