#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rheo/graphics/buffer.h"
#include "rheo/graphics/context.h"
#include "rheo/simulation/simulation_session.h"
void Check(bool v) {
  if (!v) throw std::runtime_error("GPU lattice check failed");
}
std::vector<simulation::Cell> Read(
    graphics::Device const& device, graphics::CommandPools const& pools,
    graphics::FrameSync& sync,
    simulation::LatticeRenderSnapshot const& snapshot) {
  auto const size =
      vk::DeviceSize(snapshot.cell_count) * sizeof(simulation::Cell);
  auto buffer = graphics::BufferAllocator::CreateBuffer(
      device, size, vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);
  auto command = std::move(
      device.LogicalDevice()
          .allocateCommandBuffers({.commandPool = *pools.Graphics(),
                                   .level = vk::CommandBufferLevel::ePrimary,
                                   .commandBufferCount = 1})
          .front());
  command.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command.copyBuffer(vk::Buffer(reinterpret_cast<VkBuffer>(
                         snapshot.lattice_buffer.native_handle)),
                     *buffer.buffer, vk::BufferCopy(0, 0, size));
  vk::MemoryBarrier2 barrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eHost,
      .dstAccessMask = vk::AccessFlagBits2::eHostRead};
  command.pipelineBarrier2(
      {.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});
  command.end();
  auto signal = sync.GetNextTimelineValue();
  vk::TimelineSemaphoreSubmitInfo timeline{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &snapshot.ready_signal,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signal};
  vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eTransfer;
  device.GraphicsQueue().submit(
      vk::SubmitInfo{.pNext = &timeline,
                     .waitSemaphoreCount = 1,
                     .pWaitSemaphores = &*sync.Semaphore(),
                     .pWaitDstStageMask = &stage,
                     .commandBufferCount = 1,
                     .pCommandBuffers = &*command,
                     .signalSemaphoreCount = 1,
                     .pSignalSemaphores = &*sync.Semaphore()},
      nullptr);
  sync.WaitSemaphore(device, signal);
  auto const* data =
      static_cast<simulation::Cell const*>(buffer.memory.mapMemory(0, size));
  std::vector<simulation::Cell> result(data, data + snapshot.cell_count);
  buffer.memory.unmapMemory();
  return result;
}
int main() {
  graphics::GraphicsContext context;
  graphics::Device device;
  try {
    context.Init({VK_KHR_SURFACE_EXTENSION_NAME});
    device.Init(context, nullptr);
  } catch (std::exception const& e) {
    std::cerr << "Vulkan unavailable: " << e.what() << '\n';
    return 77;
  }
  graphics::CommandPools pools;
  pools.Init(device);
  graphics::FrameSync sync;
  sync.Init(device);
  simulation::SimulationSession session(device, pools, sync);
  auto dem = std::make_shared<domain::DemData>(
      domain::DemData{.samples = {{.elevation = -2},
                                  {.elevation = -1.1F},
                                  {.elevation = -1},
                                  {.elevation = -0.01F},
                                  {.elevation = 0},
                                  {.elevation = -2}},
                      .width = 3,
                      .height = 2,
                      .pixel_size_meters = {1, 1},
                      .min_elevation = -2,
                      .max_elevation = 0});
  for (int scale : {1, 2}) {
    dem->pixel_size_meters = {scale, scale};
    auto initialized = session.InitializeTerrain(
        dem, {.height_subdivisions = 2, .upper_elevation_margin = 1});
    if (!initialized) throw std::runtime_error(initialized.error());
    auto snapshot = *session.Update(0);
    Check(snapshot.lattice_width == 3U * scale &&
          snapshot.lattice_height == 3 && snapshot.lattice_depth == 2U * scale);
    auto cells = Read(device, pools, sync, snapshot);
    std::array highest = {0, 0, 1, 1, 2, 0};
    for (unsigned z = 0; z < snapshot.lattice_depth; ++z)
      for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < snapshot.lattice_width; ++x) {
          auto const& cell = cells[x + y * snapshot.lattice_width +
                                   z * snapshot.lattice_width * 3];
          auto expected =
              y <= static_cast<unsigned>(highest[(x / scale) + (z / scale) * 3])
                  ? simulation::CellType::kObstacleTerrain
                  : simulation::CellType::kGas;
          Check(cell.type == expected &&
                cell.position == glm::vec4(x, y, z, 0));
          Check(cell.velocity == glm::vec4(0) && cell.density == 0 &&
                cell.mass == 0 && cell.pressure == 0);
        }
    auto const signal = sync.CurrentTimelineValue();
    for (int i = 0; i < 10; ++i)
      Check(session.Update(16)->ready_signal == snapshot.ready_signal);
    Check(sync.CurrentTimelineValue() == signal);
    Check(!session.InitializeTerrain(dem, {.height_subdivisions = 0}));
    // This definition fits the 32-bit cell count but exceeds the Vulkan
    // storage-buffer range even on devices exposing the largest possible limit.
    Check(!session.InitializeTerrain(dem, {.height_subdivisions = 500}));
    Check(session.Update(0)->lattice_buffer.native_handle ==
          snapshot.lattice_buffer.native_handle);
  }
  dem->max_elevation = dem->min_elevation;
  for (auto& sample : dem->samples) sample.elevation = dem->min_elevation;
  Check(session.InitializeTerrain(dem, {}).has_value());
  auto flat = *session.Update(0);
  auto cells = Read(device, pools, sync, flat);
  for (unsigned z = 0; z < flat.lattice_depth; ++z)
    for (unsigned y = 0; y < flat.lattice_height; ++y)
      for (unsigned x = 0; x < flat.lattice_width; ++x)
        Check(cells[x + y * flat.lattice_width +
                    z * flat.lattice_width * flat.lattice_height]
                  .type == (y == 0 ? simulation::CellType::kObstacleTerrain
                                   : simulation::CellType::kGas));
  // Start a basin with a compact, GPU-produced baseline.
  auto basin = std::make_shared<domain::DemData>();
  basin->width = 7;
  basin->height = 7;
  basin->pixel_size_meters = {1, 1};
  basin->min_elevation = 0;
  basin->max_elevation = 4;
  for (int z = 0; z < 7; ++z)
    for (int x = 0; x < 7; ++x)
      basin->samples.push_back(
          {.coordinate = {x, z},
           .elevation = (x == 0 || z == 0 || x == 6 || z == 6) ? 4.0F : 0.0F});
  domain::LatticeSettings settings{.height_subdivisions = 4,
                                   .upper_elevation_margin = 2};
  Check(session.InitializeTerrain(basin, settings).has_value());
  domain::CellTypes baseline(session.Editing().types.begin(),
                             session.Editing().types.end());
  auto verify = [&] {
    auto data = Read(device, pools, sync, *session.Update(0));
    auto state = session.Editing();
    for (unsigned i = 0; i < data.size(); ++i) {
      auto const& c = data[i];
      Check(int(c.type) == state.types[i]);
      bool liquid = c.type == domain::CellType::kFluid ||
                    c.type == domain::CellType::kInterface;
      Check(c.velocity == glm::vec4(0) && c.density == (liquid ? 1.0F : 0.0F) &&
            c.pressure == (liquid ? 1.0F / 3 : 0.0F));
      Check(c.mass == (c.type == domain::CellType::kFluid       ? 1.0F
                       : c.type == domain::CellType::kInterface ? .5F
                                                                : 0.0F));
      Check(glm::ivec3(c.position) ==
            domain::CellPosition(i, *state.definition));
    }
  };
  verify();
  session.BeginStroke();
  Check(session
            .Edit({.center = {3, 1, 3},
                   .brush = {.mode = domain::BrushMode::kWater}})
            .has_value());
  session.EndStroke();
  verify();
  Check(session.Editing().can_undo && session.Editing().changed);
  auto water = session.ExportEdits();
  Check(water && !water->runs.empty());
  Check(session.Edit({.kind = simulation::EditOperation::Kind::kUndo})
            .has_value());
  verify();
  Check(!session.Editing().changed && session.Editing().can_redo);
  Check(session.Edit({.kind = simulation::EditOperation::Kind::kRedo})
            .has_value());
  verify();
  auto before_signal = session.Update(0)->ready_signal;
  session.BeginStroke();
  Check(session
            .Edit({.center = {3, 0, 3},
                   .brush = {.mode = domain::BrushMode::kErase}})
            .has_value());
  session.EndStroke();
  Check(session.Update(0)->ready_signal == before_signal);
  session.BeginStroke();
  Check(session
            .Edit({.center = {2, 1, 2},
                   .brush = {.mode = domain::BrushMode::kElevation,
                             .radius = 1,
                             .elevation = 2}})
            .has_value());
  Check(session
            .Edit({.center = {3, 1, 2},
                   .brush = {.mode = domain::BrushMode::kElevation,
                             .radius = 1,
                             .elevation = 3}})
            .has_value());
  session.EndStroke();
  verify();
  auto final = session.ExportEdits();
  Check(session.Edit({.kind = simulation::EditOperation::Kind::kRestore})
            .has_value());
  verify();
  Check(!session.Editing().can_undo && !session.Editing().changed);
  Check(session.InitializeTerrain(basin, settings, final).has_value());
  verify();
  Check(session.ExportEdits()->runs == final->runs &&
        !session.Editing().can_undo);
  auto previous = *session.Update(0);
  auto invalid = *final;
  invalid.dem_fingerprint = "wrong";
  Check(!session.InitializeTerrain(basin, settings, invalid));
  Check(session.Update(0)->lattice_buffer.native_handle ==
        previous.lattice_buffer.native_handle);
  verify();
  auto changed_dem = std::make_shared<domain::DemData>(*basin);
  changed_dem->samples[8].elevation = .1F;
  Check(!session.InitializeTerrain(changed_dem, settings, final));
  Check(session.Update(0)->lattice_buffer.native_handle ==
        previous.lattice_buffer.native_handle);
  verify();
  invalid = *final;
  invalid.runs = {{0, 1, domain::Type(domain::CellType::kGas)}};
  Check(!session.InitializeTerrain(basin, settings, invalid));
  Check(session.Update(0)->lattice_buffer.native_handle ==
        previous.lattice_buffer.native_handle);
  verify();
  session.Clear();
  Check(!session.IsReady() && !session.Update(0));
  std::cout << "GPU lattice readback passed on "
            << device.PhysicalDevice().getProperties().deviceName.data()
            << '\n';
}
