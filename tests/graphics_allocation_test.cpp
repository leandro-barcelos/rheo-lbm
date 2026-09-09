#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rheo/graphics/buffer.h"
#include "rheo/graphics/images.h"

namespace {
void Check(bool condition) {
  if (!condition) throw std::runtime_error("VMA allocation check failed");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL ValidationMessage(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT,
    vk::DebugUtilsMessengerCallbackDataEXT const* data, void* user_data) {
  if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
    ++*static_cast<std::atomic_uint*>(user_data);
    std::cerr << data->pMessage << '\n';
  }
  return VK_FALSE;
}

void CheckAllocations(graphics::Device const& device) {
  graphics::CommandPools pools;
  pools.Init(device);
  VmaTotalStatistics before{};
  vmaCalculateStatistics(device.Allocator(), &before);
  {
    constexpr std::array<std::uint32_t, 4> values{1, 42, 0xabcdef, 7};
    auto buffer = graphics::BufferAllocator::CreateBuffer(
        device, sizeof(values), vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible);
    buffer.WriteMapped(values.data(), sizeof(values));
    auto handle = *buffer.Buffer();
    auto mapped = buffer.Mapped(sizeof(values));
    auto moved = std::move(buffer);
    Check(*buffer.Buffer() == nullptr);
    Check(*moved.Buffer() == handle && moved.Mapped(sizeof(values)) == mapped);
    auto replacement = graphics::BufferAllocator::CreateMappedUniformBuffer(
        device, sizeof(values));
    replacement = std::move(moved);
    Check(*moved.Buffer() == nullptr);
    auto const* read = static_cast<std::uint32_t const*>(
        replacement.ReadMapped(sizeof(values)));
    for (std::size_t i = 0; i < values.size(); ++i) Check(read[i] == values[i]);

    bool rejected = false;
    try {
      replacement.WriteMapped(values.data(), sizeof(values) + 1);
    } catch (std::runtime_error const&) {
      rejected = true;
    }
    Check(rejected);
    auto uniform =
        graphics::BufferAllocator::CreateUniformBuffer(device, pools, values);
    auto storage = graphics::BufferAllocator::CreateSSBO(
        device, pools, std::vector<std::uint32_t>{1, 2, 3}, false);
    Check(*storage[0].Buffer() != nullptr && *storage[1].Buffer() == nullptr);

    auto image = graphics::ImageAllocator::CreateSolidColorImage(
        device, pools, 12, 34, 56, 255);
    auto image_handle = *image.Image();
    auto moved_image = std::move(image);
    Check(*image.Image() == nullptr);
    Check(*moved_image.Image() == image_handle);
    auto replacement_image = graphics::ImageAllocator::CreateSolidColorImage(
        device, pools, 255, 0, 0, 255);
    replacement_image = std::move(moved_image);
    Check(*moved_image.ImageView() == nullptr);
    Check(*replacement_image.Image() == image_handle);
    Check(*replacement_image.ImageView() != nullptr);
    Check(*replacement_image.Sampler() != nullptr);
    replacement_image = {};
    replacement = {};
    Check(*replacement.Buffer() == nullptr);
    rejected = false;
    try {
      (void)replacement.Mapped(sizeof(values));
    } catch (std::runtime_error const&) {
      rejected = true;
    }
    Check(rejected);
    device.LogicalDevice().waitIdle();
  }
  VmaTotalStatistics after{};
  vmaCalculateStatistics(device.Allocator(), &after);
  Check(after.total.statistics.allocationCount ==
        before.total.statistics.allocationCount);
  Check(after.total.statistics.allocationBytes ==
        before.total.statistics.allocationBytes);
}
}  // namespace

static_assert(!std::is_copy_constructible_v<graphics::AllocatedBuffer>);
static_assert(!std::is_copy_constructible_v<graphics::AllocatedImage>);
static_assert(!std::is_copy_constructible_v<graphics::MemoryAllocator>);

int main() {
  graphics::GraphicsContext context;
  try {
    context.Init(
        {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
  } catch (std::exception const& error) {
    std::cerr << "Vulkan unavailable: " << error.what() << '\n';
    return 77;
  }
  std::atomic_uint validation_errors{0};
  vk::raii::DebugUtilsMessengerEXT messenger(
      context.Instance(),
      {.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
       .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
       .pfnUserCallback = ValidationMessage,
       .pUserData = &validation_errors});
  {
    graphics::Device device;
    device.Init(context, nullptr);
    CheckAllocations(device);
  }
  Check(validation_errors == 0);
  std::cout << "VMA allocation, mapping, moves, and cleanup passed\n";
}
