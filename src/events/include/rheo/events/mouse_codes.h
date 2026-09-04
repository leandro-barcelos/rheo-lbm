#ifndef RHEOLBM_MOUSE_CODES_H
#define RHEOLBM_MOUSE_CODES_H

#include <cstdint>

namespace events {

enum MouseCode : uint8_t {
  kButton1 = 0,
  kButton2 = 1,
  kButton3 = 2,
  kButton4 = 3,
  kButton5 = 4,
  kButton6 = 5,
  kButton7 = 6,
  kButton8 = 7,
  kButtonLast = kButton8,
  kButtonLeft = kButton1,
  kButtonRight = kButton2,
  kButtonMiddle = kButton3,
};

}  // namespace events

#endif  // !RHEOLBM_MOUSE_CODES_H
