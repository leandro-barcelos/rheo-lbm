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
  session.Clear();
  Check(!session.IsReady() && !session.Update(0));
  std::cout << "GPU lattice readback passed on "
            << device.PhysicalDevice().getProperties().deviceName.data()
            << '\n';
}
