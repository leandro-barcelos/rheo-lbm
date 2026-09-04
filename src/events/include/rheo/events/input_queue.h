#ifndef RHEO_EVENTS_INPUT_QUEUE_H
#define RHEO_EVENTS_INPUT_QUEUE_H

#include <utility>
#include <vector>

#include "rheo/events/input_event.h"

namespace events {

class InputQueue {
 public:
  void Push(InputEvent event) { events_.push_back(std::move(event)); }

  [[nodiscard]] std::vector<InputEvent> Drain() {
    std::vector<InputEvent> result;
    result.swap(events_);
    return result;
  }

  [[nodiscard]] bool Empty() const { return events_.empty(); }

 private:
  std::vector<InputEvent> events_;
};

}  // namespace events

#endif  // RHEO_EVENTS_INPUT_QUEUE_H
