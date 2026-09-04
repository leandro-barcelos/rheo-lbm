#ifndef RHEO_DOMAIN_IMAGE_DATA_H
#define RHEO_DOMAIN_IMAGE_DATA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace domain {

struct ImageData {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;

  [[nodiscard]] bool IsValid() const {
    return width > 0 && height > 0 &&
           rgba.size() == static_cast<std::size_t>(width) * height * 4U;
  }
};

using SharedImage = std::shared_ptr<const ImageData>;

}  // namespace domain

#endif  // RHEO_DOMAIN_IMAGE_DATA_H
